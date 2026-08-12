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
