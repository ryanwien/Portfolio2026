/* Shared behaviour for the space theme: scroll reveals, the rotating role
   word, and motion safety. Everything here is progressive enhancement — the
   reveal class is only added from JS, so content is never hidden if this file
   fails to run. */
(function () {
  var reduce = window.matchMedia('(prefers-reduced-motion: reduce)').matches;

  document.addEventListener('DOMContentLoaded', function () {
    /* Vestibular safety: model-viewer has no built-in reduced-motion handling,
       so spinning is opt-out here — models stay interactive, just still. */
    if (reduce) {
      document.querySelectorAll('model-viewer[auto-rotate]').forEach(function (mv) {
        mv.removeAttribute('auto-rotate');
      });
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
            entry.target.classList.add('site-in');
            io.unobserve(entry.target);
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
    }

    /* Rotating role word: <span data-rotate='["a","b"]'>a</span> */
    document.querySelectorAll('[data-rotate]').forEach(function (el) {
      var words;
      try { words = JSON.parse(el.getAttribute('data-rotate')); } catch (e) { return; }
      if (!Array.isArray(words) || words.length < 2) return;
      var i = 0;
      setInterval(function () {
        i = (i + 1) % words.length;
        el.textContent = words[i];
        el.classList.remove('site-role-swap');
        void el.offsetWidth; /* restart the animation */
        el.classList.add('site-role-swap');
      }, 2200);
    });
  });
})();
