/* Shared behaviour for the space theme: scroll reveals, the rotating role
   word, and motion safety. Everything here is progressive enhancement — the
   reveal class is only added from JS, so content is never hidden if this file
   fails to run. */
(function () {
  var motionQuery = window.matchMedia('(prefers-reduced-motion: reduce)');

  /* Stop every 3D emblem spinning. Called on load and again if the visitor
     turns the preference on mid-visit — model-viewer has no reduced-motion
     handling of its own, so this is the only thing holding the models still. */
  function stopRotation() {
    document.querySelectorAll('model-viewer[auto-rotate]').forEach(function (mv) {
      mv.removeAttribute('auto-rotate');
    });
  }

  /* Drop the reveal classes once the element has arrived. Leaving them on
     would keep a filled animation sitting in the cascade on top of whatever
     the element does next — most visibly .site-card's hover lift. */
  function settle(el) {
    el.classList.remove('site-reveal', 'site-in');
    el.style.removeProperty('--reveal-delay');
  }

  function reveal(el) {
    el.classList.add('site-in');
    el.addEventListener('animationend', function handler(e) {
      if (e.animationName !== 'site-reveal-in') return;
      el.removeEventListener('animationend', handler);
      settle(el);
    });
  }

  if (motionQuery.addEventListener) {
    motionQuery.addEventListener('change', function (e) {
      if (!e.matches) return;
      stopRotation();
      document.querySelectorAll('.site-reveal').forEach(settle);
    });
  }

  /* First-visit launch sequence. Runs only when the page's head guard put
     .launching on <html> (index.html, motion allowed, not seen this session).
     Counter 000→100 with rotating words, then the overlay fades as the hero
     entrance plays. */
  function runLaunch() {
    var root = document.documentElement;
    if (!root.classList.contains('launching')) return;

    var overlay = document.createElement('div');
    overlay.className = 'site-launch';
    overlay.setAttribute('aria-hidden', 'true');
    overlay.innerHTML =
      '<div class="site-launch-label">Portfolio · 2026</div>' +
      '<div class="site-launch-word site-accent-word">Design</div>' +
      '<div class="site-launch-count">000</div>' +
      '<div class="site-launch-bar"><span></span></div>';
    document.body.appendChild(overlay);

    var wordEl = overlay.querySelector('.site-launch-word');
    var countEl = overlay.querySelector('.site-launch-count');
    var barEl = overlay.querySelector('.site-launch-bar span');

    var words = ['Design', 'Build', 'Launch'];
    var wi = 0;
    var wordTimer = setInterval(function () {
      wi = (wi + 1) % words.length;
      wordEl.textContent = words[wi];
      wordEl.classList.remove('site-role-swap');
      void wordEl.offsetWidth;
      wordEl.classList.add('site-role-swap');
    }, 850);

    var DURATION = 2400;
    var t0 = performance.now();
    function frame(now) {
      var p = Math.min(1, (now - t0) / DURATION);
      var eased = p < 0.5 ? 2 * p * p : 1 - Math.pow(-2 * p + 2, 2) / 2;
      var count = Math.round(eased * 100);
      countEl.textContent = ('00' + count).slice(-3);
      barEl.style.transform = 'scaleX(' + count / 100 + ')';
      if (p < 1) { requestAnimationFrame(frame); return; }

      clearInterval(wordTimer);
      setTimeout(function () {
        try { sessionStorage.setItem('rw-launch-seen', '1'); } catch (e) {}
        root.classList.remove('launching');        /* hero entrance starts   */
        overlay.classList.add('site-launch-done'); /* ...as the overlay fades */
        overlay.addEventListener('transitionend', function () { overlay.remove(); });
        setTimeout(function () { if (overlay.parentNode) overlay.remove(); }, 1200);
      }, 350);
    }
    requestAnimationFrame(frame);
  }

  /* Ambient sound: a synthesized deep-space drone, built entirely from the
     Web Audio API — no file to download, no loop seam. Off by default; the
     nav toggle is the only way in, which also satisfies browser autoplay
     rules (the first start always rides a user gesture). */
  var audio = null;
  var soundActive = false;
  var SOUND_LEVEL = 0.35;

  function buildAudio() {
    var Ctx = window.AudioContext || window.webkitAudioContext;
    if (!Ctx) return null;
    var ctx = new Ctx();
    var master = ctx.createGain();
    master.gain.value = 0;
    master.connect(ctx.destination);

    /* A feedback delay stands in for reverb: anything sent here repeats
       every 0.44s a little darker and quieter, which reads as the size of
       the room — and the room is enormous. */
    var echo = ctx.createDelay(1);
    echo.delayTime.value = 0.44;
    var echoTone = ctx.createBiquadFilter();
    echoTone.type = 'lowpass';
    echoTone.frequency.value = 2400;
    var echoFb = ctx.createGain();
    echoFb.gain.value = 0.38;
    echo.connect(echoTone);
    echoTone.connect(echoFb);
    echoFb.connect(echo);
    var echoOut = ctx.createGain();
    echoOut.gain.value = 0.5;
    echoTone.connect(echoOut);
    echoOut.connect(master);

    /* Everything sustained plays into a "bed" whose level breathes very
       slowly, so the pad swells and recedes instead of holding one static
       tone. The master above it stays clean for the on/off fades. */
    var bed = ctx.createGain();
    bed.gain.value = 0.9;
    bed.connect(master);
    var breath = ctx.createOscillator();
    breath.frequency.value = 0.06;
    var breathDepth = ctx.createGain();
    breathDepth.gain.value = 0.08;
    breath.connect(breathDepth);
    breathDepth.connect(bed.gain);
    breath.start();

    /* The pad: A-major add9, voiced mid-register so it floats instead of
       looming — the ninth (B4) and the faint major third (C#5) are what keep
       it hopeful rather than ominous. A light octave floor underneath gives
       headphones some warmth without dragging the mood down. The fractional
       detunes make layers beat slowly instead of sounding like an organ. */
    [
      [55, 0.2], [110, 0.15],
      [220, 0.14], [220.6, 0.1],
      [329.6, 0.12], [330.4, 0.08],
      [440, 0.09],
      [493.9, 0.07],
      [554.4, 0.05]
    ].forEach(function (pair) {
      var osc = ctx.createOscillator();
      osc.type = 'sine';
      osc.frequency.value = pair[0];
      var g = ctx.createGain();
      g.gain.value = pair[1];
      osc.connect(g);
      g.connect(bed);
      osc.start();
    });

    /* Cosmic air: looping noise through a high band-pass whose centre
       wanders on a very slow LFO — a thin bright breeze, not a rumble. */
    var len = ctx.sampleRate * 2;
    var buf = ctx.createBuffer(1, len, ctx.sampleRate);
    var data = buf.getChannelData(0);
    for (var i = 0; i < len; i++) data[i] = Math.random() * 2 - 1;
    var noise = ctx.createBufferSource();
    noise.buffer = buf;
    noise.loop = true;
    var band = ctx.createBiquadFilter();
    band.type = 'bandpass';
    band.frequency.value = 1400;
    band.Q.value = 0.8;
    var windGain = ctx.createGain();
    windGain.gain.value = 0.2;
    noise.connect(band);
    band.connect(windGain);
    windGain.connect(bed);
    noise.start();
    var lfo = ctx.createOscillator();
    lfo.frequency.value = 0.045;
    var lfoDepth = ctx.createGain();
    lfoDepth.gain.value = 600;
    lfo.connect(lfoDepth);
    lfoDepth.connect(band.frequency);
    lfo.start();

    /* Star pings: every few seconds a soft pentatonic chime rings out from a
       random point in the stereo field and trails away through the echo. */
    var PENTA = [659.3, 880, 987.8, 1108.7, 1318.5];
    function ping() {
      if (!soundActive || ctx.state !== 'running') return;
      var t = ctx.currentTime;
      var osc = ctx.createOscillator();
      osc.type = 'sine';
      osc.frequency.value = PENTA[Math.floor(Math.random() * PENTA.length)];
      var g = ctx.createGain();
      g.gain.setValueAtTime(0.0001, t);
      g.gain.exponentialRampToValueAtTime(0.05 + Math.random() * 0.05, t + 0.02);
      g.gain.exponentialRampToValueAtTime(0.0001, t + 1.8);
      osc.connect(g);
      var out = g;
      if (ctx.createStereoPanner) {
        var pan = ctx.createStereoPanner();
        pan.pan.value = Math.random() * 1.6 - 0.8;
        g.connect(pan);
        out = pan;
      }
      out.connect(master);
      out.connect(echo);
      osc.start(t);
      osc.stop(t + 2);
    }
    setInterval(function () { if (Math.random() < 0.65) ping(); }, 2600);

    return { ctx: ctx, master: master, echo: echo };
  }

  function soundOn() {
    if (!audio) audio = buildAudio();
    if (!audio) return false;
    soundActive = true;
    audio.ctx.resume();
    var g = audio.master.gain;
    g.cancelScheduledValues(audio.ctx.currentTime);
    g.setValueAtTime(g.value, audio.ctx.currentTime);
    g.linearRampToValueAtTime(SOUND_LEVEL, audio.ctx.currentTime + 1.4);
    return true;
  }

  function soundOff() {
    if (!audio) return;
    soundActive = false;
    var g = audio.master.gain;
    g.cancelScheduledValues(audio.ctx.currentTime);
    g.setValueAtTime(g.value, audio.ctx.currentTime);
    g.linearRampToValueAtTime(0, audio.ctx.currentTime + 0.6);
    /* Suspend once the fade lands, unless it was switched back on meanwhile —
       a suspended context costs nothing while the visitor reads in silence. */
    setTimeout(function () {
      if (audio && !soundActive) audio.ctx.suspend();
    }, 700);
  }

  /* UI tick: a soft console blip for clicks on pills, nav links, and the
     toggle itself. Gated behind the ambient switch, so the toggle stays the
     one master control for every sound the site makes. */
  function tick() {
    if (!audio || !soundActive) return;
    var ctx = audio.ctx;
    var t = ctx.currentTime;
    var osc = ctx.createOscillator();
    osc.type = 'sine';
    osc.frequency.setValueAtTime(1320, t);
    osc.frequency.exponentialRampToValueAtTime(880, t + 0.06);
    var g = ctx.createGain();
    g.gain.setValueAtTime(0.0001, t);
    g.gain.exponentialRampToValueAtTime(0.3, t + 0.008);
    g.gain.exponentialRampToValueAtTime(0.0001, t + 0.09);
    osc.connect(g);
    g.connect(audio.master);
    if (audio.echo) g.connect(audio.echo); /* the blip ricochets away */
    osc.start(t);
    osc.stop(t + 0.1);
  }

  function initSound() {
    var nav = document.querySelector('.site-nav-inner');
    if (!nav) return;

    var btn = document.createElement('button');
    btn.className = 'site-sound';
    btn.type = 'button';
    btn.setAttribute('aria-pressed', 'false');
    btn.setAttribute('aria-label', 'Ambient sound');
    btn.title = 'Ambient sound';
    btn.innerHTML = '<span></span><span></span><span></span><span></span>';
    nav.appendChild(btn);

    var on = false;
    function set(state) {
      if (state && !soundOn()) state = false; /* no Web Audio support */
      if (!state) soundOff();
      on = state;
      btn.classList.toggle('on', on);
      btn.setAttribute('aria-pressed', String(on));
      try { localStorage.setItem('rw-sound', on ? 'on' : 'off'); } catch (e) {}
    }
    btn.addEventListener('click', function () { set(!on); });

    /* Delegated after the toggle's own handler, so switching ON blips a
       confirmation and switching OFF stays silent. */
    document.addEventListener('click', function (e) {
      if (e.target.closest && e.target.closest('.btn, .site-link, .site-brand, .site-sound')) tick();
    });

    /* The visitor opted in on an earlier page. The browser still demands a
       gesture on THIS page before audio may start, so arm a one-shot listener
       — unless the gesture is the toggle itself, whose click handler decides. */
    var pref = null;
    try { pref = localStorage.getItem('rw-sound'); } catch (e) {}
    if (pref === 'on') {
      var disarm = function () {
        window.removeEventListener('pointerdown', resume);
        window.removeEventListener('keydown', resume);
      };
      var resume = function (e) {
        disarm();
        if (!btn.contains(e.target)) set(true);
      };
      window.addEventListener('pointerdown', resume);
      window.addEventListener('keydown', resume);
    }
  }

  /* Magnetic pills: the homepage project buttons lean toward the cursor and
     spring back when it leaves. The .btn transform transition supplies the
     elasticity. Fine pointers only — touch never hovers. */
  function initMagnet() {
    if (!window.matchMedia('(pointer:fine)').matches) return;
    document.querySelectorAll('.proj .btn').forEach(function (btn) {
      btn.addEventListener('pointermove', function (e) {
        if (motionQuery.matches) return;
        var r = btn.getBoundingClientRect();
        var dx = e.clientX - (r.left + r.width / 2);
        var dy = e.clientY - (r.top + r.height / 2);
        btn.style.transform =
          'translate(' + (dx * 0.18).toFixed(1) + 'px,' +
          (dy * 0.3 - 2).toFixed(1) + 'px) scale(1.03)';
      });
      btn.addEventListener('pointerleave', function () {
        btn.style.transform = '';
      });
    });
  }

  /* Sound stays available under reduced motion — audio is opt-in and not
     motion — so it initializes outside the early return below. */
  document.addEventListener('DOMContentLoaded', function () {
    initSound();
    initMagnet();
  });

  document.addEventListener('DOMContentLoaded', function () {
    if (motionQuery.matches) {
      stopRotation();
      document.documentElement.classList.remove('launching');
      return; /* no reveals, no rotator, no launch — the page simply appears */
    }

    runLaunch();

    /* Scroll-story parallax: keep --scroll at scrollY (unitless; the CSS
       multiplies it into px). rAF-throttled, transform-only consumers. */
    var pending = false;
    function onScroll() {
      if (pending) return;
      pending = true;
      requestAnimationFrame(function () {
        document.documentElement.style.setProperty('--scroll', window.scrollY);
        pending = false;
      });
    }
    window.addEventListener('scroll', onScroll, { passive: true });
    onScroll();

    /* Scroll reveals. Tag the shared building blocks plus anything opting in
       with data-reveal, then stagger siblings that share a parent. */
    var targets = document.querySelectorAll(
      '.site-card, .proj, .site-chips, .site-footer-inner > *, [data-reveal]'
    );
    if ('IntersectionObserver' in window && targets.length) {
      var io = new IntersectionObserver(function (entries) {
        entries.forEach(function (entry) {
          if (entry.isIntersecting) {
            io.unobserve(entry.target);
            reveal(entry.target);
          }
        });
      }, { rootMargin: '0px 0px -70px 0px', threshold: 0.05 });

      targets.forEach(function (el) {
        var siblings = el.parentElement ? Array.prototype.slice.call(el.parentElement.children) : [];
        var index = Math.max(0, siblings.indexOf(el));
        el.style.setProperty('--reveal-delay', Math.min(index * 80, 320) + 'ms');
        el.classList.add('site-reveal');
        io.observe(el);
      });

      /* Tabbing runs ahead of scrolling, and the observer hasn't fired for
         rows further down the page yet. Without this, keyboard users can land
         on a link inside a block that is still at opacity:0. */
      document.addEventListener('focusin', function (e) {
        var hidden = e.target.closest && e.target.closest('.site-reveal:not(.site-in)');
        if (hidden) {
          io.unobserve(hidden);
          settle(hidden);
        }
      });
    }

    /* Rotating word: <span data-rotate='["a","b"]'>a</span> */
    document.querySelectorAll('[data-rotate]').forEach(function (el) {
      var words;
      try { words = JSON.parse(el.getAttribute('data-rotate')); } catch (e) { return; }
      if (!Array.isArray(words) || words.length < 2) return;

      /* Reserve the widest word so the surrounding line never reflows. */
      var original = el.textContent;
      var widest = 0;
      words.forEach(function (w) {
        el.textContent = w;
        widest = Math.max(widest, el.getBoundingClientRect().width);
      });
      el.textContent = original;
      el.style.minWidth = Math.ceil(widest) + 'px';

      var i = 0;
      var timer = null;
      function swap() {
        i = (i + 1) % words.length;
        el.textContent = words[i];
        el.classList.remove('site-role-swap');
        void el.offsetWidth; /* restart the animation */
        el.classList.add('site-role-swap');
      }
      function start() { if (!timer) timer = setInterval(swap, 2200); }
      function stop() { clearInterval(timer); timer = null; }

      start();
      /* A background tab shouldn't keep repainting a gradient word. */
      document.addEventListener('visibilitychange', function () {
        if (document.hidden) stop(); else start();
      });
    });
  });
})();
