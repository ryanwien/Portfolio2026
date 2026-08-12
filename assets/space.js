/* Shared behaviour for the 3D emblems.
   Vestibular safety: model-viewer has no built-in reduced-motion handling, so
   spinning is opt-out here — the models stay interactive, they just hold still. */
if (window.matchMedia('(prefers-reduced-motion: reduce)').matches) {
  document.addEventListener('DOMContentLoaded', function () {
    document.querySelectorAll('model-viewer[auto-rotate]').forEach(function (mv) {
      mv.removeAttribute('auto-rotate');
    });
  });
}
