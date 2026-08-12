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

  document.addEventListener('DOMContentLoaded', function () {
    if (motionQuery.matches) {
      stopRotation();
      return; /* no reveals, no rotator — the page simply appears */
    }

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
