/* Shared behaviour for the space theme: scroll reveals, the rotating role
   word, and motion safety. Everything here is progressive enhancement — the
   reveal class is only added from JS, so content is never hidden if this file
   fails to run. */
(function () {
  var motionQuery = window.matchMedia('(prefers-reduced-motion: reduce)');

  /* The 3D emblems live in space3d.js, which handles its own motion and
     reduced-motion behaviour. */

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

  /* Live level for anything that wants to move with the music (the hero
     planet's atmosphere reads this from space3d.js). Returns 0..1-ish from
     the low end of the spectrum, already smoothed by the analyser. */
  var analyser = null;
  var analyserBuf = null;
  window.rwAudioLevel = function () {
    if (!analyser || !soundActive) return 0;
    analyser.getByteFrequencyData(analyserBuf);
    var sum = 0;
    for (var i = 0; i < 20; i++) sum += analyserBuf[i];
    return sum / (20 * 255);
  };

  function buildAudio() {
    var Ctx = window.AudioContext || window.webkitAudioContext;
    if (!Ctx) return null;
    var ctx = new Ctx();
    var master = ctx.createGain();
    master.gain.value = 0;
    master.connect(ctx.destination);

    analyser = ctx.createAnalyser();
    analyser.fftSize = 128;
    analyser.smoothingTimeConstant = 0.85;
    analyserBuf = new Uint8Array(analyser.frequencyBinCount);
    master.connect(analyser); /* a tap, not a link in the chain */

    /* A feedback delay stands in for reverb: anything sent here repeats a
       little darker and quieter, which reads as the size of the room — and
       the room is enormous. 0.63s is a dotted eighth of the arpeggio's
       0.42s step, the classic setting that turns plucks into phrases. */
    var echo = ctx.createDelay(1);
    echo.delayTime.value = 0.63;
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

    /* The bed is four chords, not one: A add9 → F#m11 → Dmaj9 → E add9,
       crossfaded every eight seconds so the music is always travelling
       somewhere. Each chord is a bank of triangles plus a sine sub on the
       root; the scheduler further down fades the banks in and out. */
    var CHORDS = [
      [110.0, 164.81, 246.94, 277.18],
      [92.5, 138.59, 164.81, 246.94],
      [146.83, 185.0, 220.0, 329.63],
      [82.41, 207.65, 246.94, 329.63]
    ];
    var CHORD_LEN = 8;
    var chordBanks = CHORDS.map(function (tones) {
      var bank = ctx.createGain();
      bank.gain.value = 0;
      bank.connect(bed);
      tones.forEach(function (f, i) {
        var osc = ctx.createOscillator();
        osc.type = 'triangle';
        osc.frequency.value = f;
        var g = ctx.createGain();
        g.gain.value = i === 0 ? 0.11 : 0.065;
        osc.connect(g);
        g.connect(bank);
        osc.start();
      });
      var sub = ctx.createOscillator();
      sub.type = 'sine';
      sub.frequency.value = tones[0] / 2;
      var sg = ctx.createGain();
      sg.gain.value = 0.22;
      sub.connect(sg);
      sg.connect(bank);
      sub.start();
      return bank;
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
    windGain.gain.value = 0.12; /* quieter now that the music carries it */
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

    /* The melody: a generative arpeggio over whichever chord is sounding.
       Each step it either rests or plucks a chord tone an octave or two up
       from a random point in the stereo field; the dotted-eighth echo turns
       the plucks into phrases. A short lookahead keeps the scheduling
       sample-accurate without queueing far ahead. */
    var STEP = 0.42;
    var chordIdx = -1;
    var nextNote = 0;
    var nextChord = 0;
    function pluck(freq, t) {
      var osc = ctx.createOscillator();
      osc.type = 'triangle';
      osc.frequency.value = freq;
      var g = ctx.createGain();
      g.gain.setValueAtTime(0.0001, t);
      g.gain.exponentialRampToValueAtTime(0.17, t + 0.015);
      g.gain.exponentialRampToValueAtTime(0.0001, t + 1.3);
      var lp = ctx.createBiquadFilter();
      lp.type = 'lowpass';
      lp.frequency.value = 2200;
      osc.connect(g);
      g.connect(lp);
      var out = lp;
      if (ctx.createStereoPanner) {
        var pan = ctx.createStereoPanner();
        pan.pan.value = Math.random() * 1.2 - 0.6;
        lp.connect(pan);
        out = pan;
      }
      out.connect(bed);
      out.connect(echo);
      osc.start(t);
      osc.stop(t + 1.4);
    }
    setInterval(function () {
      if (!soundActive || ctx.state !== 'running') { nextNote = 0; return; }
      var now = ctx.currentTime;
      if (!nextNote) { nextNote = now + 0.15; nextChord = now; }
      while (nextChord <= now + 0.6) {
        chordIdx = (chordIdx + 1) % CHORDS.length;
        var t = Math.max(now, nextChord);
        chordBanks.forEach(function (bank, i) {
          bank.gain.cancelScheduledValues(t);
          bank.gain.setValueAtTime(bank.gain.value, t);
          bank.gain.linearRampToValueAtTime(i === chordIdx ? 1 : 0, t + 2.5);
        });
        nextChord = t + CHORD_LEN;
      }
      while (nextNote <= now + 0.6) {
        if (Math.random() < 0.7) {
          var tones = CHORDS[Math.max(0, chordIdx)];
          var f = tones[1 + Math.floor(Math.random() * (tones.length - 1))] *
            (Math.random() < 0.6 ? 2 : 4);
          pluck(f, nextNote);
        }
        nextNote += STEP;
      }
    }, 200);

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

  /* Footer deploy line: ask GitHub for the newest commit on main and append
     its date to the footer note. Cached per tab so hopping between pages
     costs one API call, not six (unauthenticated limit is 60/hour). */
  function initDeployLine() {
    var note = document.querySelector('.site-footer-note');
    if (!note || typeof fetch !== 'function') return;

    function show(iso) {
      var then = new Date(iso);
      if (isNaN(then)) return;
      var days = Math.floor((Date.now() - then.getTime()) / 86400000);
      var when;
      if (days <= 0) when = 'today';
      else if (days === 1) when = 'yesterday';
      else if (days < 7) when = days + ' days ago';
      else when = then.toLocaleDateString('en-US', { month: 'short', day: 'numeric', year: 'numeric' });
      var span = document.createElement('span');
      span.textContent = ' · last deploy ' + when;
      note.appendChild(span);
    }

    var cached = null;
    try { cached = sessionStorage.getItem('rw-deploy'); } catch (e) {}
    if (cached) { show(cached); return; }

    fetch('https://api.github.com/repos/ryanwien/Portfolio2026/commits/main')
      .then(function (res) { return res.ok ? res.json() : null; })
      .then(function (data) {
        var iso = data && data.commit && data.commit.committer && data.commit.committer.date;
        if (!iso) return; /* rate-limited or offline — the note reads fine without it */
        try { sessionStorage.setItem('rw-deploy', iso); } catch (e) {}
        show(iso);
      })
      .catch(function () {});
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
    initDeployLine();
  });

  document.addEventListener('DOMContentLoaded', function () {
    if (motionQuery.matches) {
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
