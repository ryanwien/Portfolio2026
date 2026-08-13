/* Procedural 3D emblems, drawn with three.js instead of shipped model files.
   Each [data-emblem] element gets a transparent WebGL canvas and a small
   stylized scene in the site palette. Everything is code — no downloads, no
   posters, and every object is designed to look right from any angle.

   Scenes pause offscreen (IntersectionObserver) and render one still frame
   under prefers-reduced-motion. Dragging an emblem hands its rotation to the
   visitor. */
import * as THREE from 'https://cdn.jsdelivr.net/npm/three@0.170.0/build/three.module.min.js';

const PALETTE = {
  accent: 0x6E86FF,
  deep: 0x5F76E8,
  nebula: 0xB48CFF,
  cream: 0xE9EEF8,
  dark: 0x1D2839,
  slate: 0x2B3A52,
};

const mat = (color, opts = {}) =>
  new THREE.MeshStandardMaterial({ color, roughness: 0.45, metalness: 0.12, ...opts });

/* Every scene returns { group, tick(seconds) }. The builders only place
   geometry; lights and camera are shared and added by init(). */
const EMBLEMS = {

  planet(scene) {
    const group = new THREE.Group();
    const planet = new THREE.Mesh(new THREE.SphereGeometry(1.5, 56, 36), mat(PALETTE.deep));
    /* A back-side additive shell reads as atmosphere and gives the smooth
       sphere its rim light. */
    const atmo = new THREE.Mesh(new THREE.SphereGeometry(1.62, 48, 32),
      new THREE.MeshBasicMaterial({
        color: PALETTE.accent, transparent: true, opacity: 0.14,
        side: THREE.BackSide, blending: THREE.AdditiveBlending, depthWrite: false,
      }));

    /* The ring is a flattened torus — real thickness, so even seen dead
       edge-on (the visitor can drag anywhere) it stays a cream band instead
       of collapsing into a grey line. */
    const rings = new THREE.Group();
    const band = new THREE.Mesh(new THREE.TorusGeometry(2.1, 0.32, 24, 128),
      mat(PALETTE.cream, { roughness: 0.4, metalness: 0.3 }));
    band.scale.z = 0.16;
    const trim = new THREE.Mesh(new THREE.TorusGeometry(2.52, 0.05, 12, 128), mat(PALETTE.accent));
    trim.scale.z = 0.5;
    rings.add(band, trim);
    rings.rotation.x = Math.PI / 2 - 0.35; /* ~20° tilt, never edge-on at rest */
    const moonA = new THREE.Mesh(new THREE.SphereGeometry(0.17, 24, 16), mat(PALETTE.nebula));
    const moonB = new THREE.Mesh(new THREE.SphereGeometry(0.11, 20, 14), mat(PALETTE.cream));
    group.add(atmo);
    group.add(planet, rings, moonA, moonB);
    group.rotation.z = -0.1;
    return {
      group,
      radius: 2.6,
      tick(s) {
        planet.rotation.y = s * 0.25;
        moonA.position.set(Math.cos(s * 0.5) * 2.55, Math.sin(s * 0.5) * 0.85, Math.sin(s * 0.5) * 1.3);
        moonB.position.set(Math.cos(s * 0.8 + 2) * 2.15, -Math.sin(s * 0.8 + 2) * 0.6, Math.sin(s * 0.8 + 2) * 1.05);
      },
    };
  },

  /* Task Tracker: a mission console with a glowing check on screen. */
  console(scene) {
    const group = new THREE.Group();
    const base = new THREE.Mesh(new THREE.BoxGeometry(2.4, 0.9, 1.5), mat(PALETTE.cream));
    base.position.y = -0.8;
    const screenBack = new THREE.Mesh(new THREE.BoxGeometry(2.1, 1.5, 0.22), mat(PALETTE.dark));
    screenBack.position.y = 0.25;
    screenBack.rotation.x = -0.16;
    const screen = new THREE.Mesh(new THREE.PlaneGeometry(1.78, 1.16),
      mat(PALETTE.accent, { flatShading: false, emissive: PALETTE.accent, emissiveIntensity: 0.55 }));
    screen.position.set(0, 0.27, 0.13);
    screen.rotation.x = -0.16;

    const checkShape = new THREE.Shape();
    checkShape.moveTo(-0.42, 0.02); checkShape.lineTo(-0.14, -0.26); checkShape.lineTo(0.42, 0.3);
    checkShape.lineTo(0.28, 0.44); checkShape.lineTo(-0.14, 0.02); checkShape.lineTo(-0.28, 0.16);
    checkShape.closePath();
    const check = new THREE.Mesh(
      new THREE.ExtrudeGeometry(checkShape, { depth: 0.12, bevelEnabled: true, bevelThickness: 0.03, bevelSize: 0.03, bevelSegments: 3 }),
      mat(PALETTE.cream, { emissive: 0xffffff, emissiveIntensity: 0.25 }));
    check.position.set(0, 0.22, 0.2);
    check.rotation.x = -0.16;

    const keys = new THREE.Mesh(new THREE.BoxGeometry(1.3, 0.14, 0.7), mat(PALETTE.slate));
    keys.position.set(0, -0.28, 0.35);
    keys.rotation.x = 0.28;
    const knobA = new THREE.Mesh(new THREE.CylinderGeometry(0.11, 0.11, 0.3, 28), mat(PALETTE.accent));
    knobA.position.set(-0.95, -0.25, 0.45);
    const knobB = knobA.clone(); knobB.material = mat(PALETTE.nebula); knobB.position.x = 0.95;
    group.add(base, screenBack, screen, check, keys, knobA, knobB);
    return {
      group,
      radius: 1.5,
      tick(s) { check.position.z = 0.2 + Math.sin(s * 1.6) * 0.05; },
    };
  },

  /* Order Book: mirrored depth-ladder towers with a spread gem in the gap. */
  ladder(scene) {
    const group = new THREE.Group();
    const plate = new THREE.Mesh(new THREE.CylinderGeometry(1.9, 2.05, 0.16, 48), mat(PALETTE.dark));
    plate.position.y = -1.15;
    group.add(plate);
    const bars = [];
    for (let i = 0; i < 5; i++) {
      const h = 0.45 + i * 0.42;
      const bid = new THREE.Mesh(new THREE.BoxGeometry(0.42, h, 0.42), mat(PALETTE.accent));
      bid.position.set(-0.55 - i * 0.48, -1.07 + h / 2, 0);
      const ask = new THREE.Mesh(new THREE.BoxGeometry(0.42, h, 0.42), mat(PALETTE.nebula));
      ask.position.set(0.55 + i * 0.48, -1.07 + h / 2, 0);
      bars.push(bid, ask);
      group.add(bid, ask);
    }
    const gem = new THREE.Mesh(new THREE.OctahedronGeometry(0.3),
      mat(PALETTE.cream, { emissive: 0xffffff, emissiveIntensity: 0.18 }));
    gem.position.y = 0.15;
    group.add(gem);
    return {
      group,
      radius: 2.75,
      tick(s) {
        gem.rotation.y = s * 0.9;
        gem.position.y = 0.15 + Math.sin(s * 1.4) * 0.1;
        bars.forEach((b, i) => { b.scale.y = 1 + Math.sin(s * 1.1 + i) * 0.035; });
      },
    };
  },

  /* ML Housing: a habitat dome with a blinking antenna. */
  dome(scene) {
    const group = new THREE.Group();
    const pad = new THREE.Mesh(new THREE.CylinderGeometry(2.0, 2.15, 0.22, 48), mat(PALETTE.dark));
    pad.position.y = -1.05;
    const domeM = new THREE.Mesh(
      new THREE.SphereGeometry(1.45, 48, 24, 0, Math.PI * 2, 0, Math.PI / 2), mat(PALETTE.cream));
    domeM.position.y = -0.95;
    const belt = new THREE.Mesh(new THREE.CylinderGeometry(1.47, 1.5, 0.24, 48), mat(PALETTE.slate));
    belt.position.y = -0.9;
    const door = new THREE.Mesh(new THREE.BoxGeometry(0.5, 0.7, 0.2), mat(PALETTE.accent));
    door.position.set(0, -0.72, 1.32);
    const mast = new THREE.Mesh(new THREE.CylinderGeometry(0.03, 0.03, 0.9, 12), mat(PALETTE.slate));
    mast.position.y = 0.9;
    const beacon = new THREE.Mesh(new THREE.SphereGeometry(0.09, 18, 12),
      mat(PALETTE.nebula, { emissive: PALETTE.nebula, emissiveIntensity: 1 }));
    beacon.position.y = 1.38;
    const pod = new THREE.Mesh(new THREE.SphereGeometry(0.42, 32, 16, 0, Math.PI * 2, 0, Math.PI / 2), mat(PALETTE.deep));
    pod.position.set(1.35, -0.95, 0.55);
    group.add(pad, domeM, belt, door, mast, beacon, pod);
    return {
      group,
      radius: 2.2,
      tick(s) { beacon.material.emissiveIntensity = 0.5 + (Math.sin(s * 2.2) + 1) * 0.45; },
    };
  },

  /* File Organizer: strapped cargo pods stacked and ready to ship. */
  pods(scene) {
    const group = new THREE.Group();
    const podMesh = (color) => {
      const p = new THREE.Group();
      const body = new THREE.Mesh(new THREE.CapsuleGeometry(0.42, 1.15, 12, 32), mat(PALETTE.cream));
      body.rotation.z = Math.PI / 2;
      const strap = new THREE.Mesh(new THREE.CylinderGeometry(0.435, 0.435, 0.16, 32), mat(color));
      strap.rotation.z = Math.PI / 2;
      const capA = new THREE.Mesh(new THREE.CylinderGeometry(0.3, 0.34, 0.1, 28), mat(color));
      capA.rotation.z = Math.PI / 2; capA.position.x = -1.02;
      const capB = capA.clone(); capB.position.x = 1.02; capB.rotation.z = -Math.PI / 2;
      p.add(body, strap, capA, capB);
      return p;
    };
    const a = podMesh(PALETTE.accent); a.position.set(0, -0.85, 0.5);
    const b = podMesh(PALETTE.nebula); b.position.set(0.15, -0.85, -0.55);
    const c = podMesh(PALETTE.deep); c.position.set(0.05, -0.05, -0.02); c.rotation.y = 0.12;
    group.add(a, b, c);
    return {
      group,
      radius: 1.4,
      tick(s) { c.position.y = -0.05 + Math.sin(s * 1.3) * 0.05; },
    };
  },

  /* Security: a guardian satellite inside its patrol ring. */
  satellite(scene) {
    const group = new THREE.Group();
    const sat = new THREE.Group();
    const body = new THREE.Mesh(new THREE.BoxGeometry(0.75, 0.75, 1.05), mat(PALETTE.cream));
    const panelGeo = new THREE.BoxGeometry(1.5, 0.05, 0.75);
    const panelA = new THREE.Mesh(panelGeo, mat(PALETTE.accent, { metalness: 0.4, roughness: 0.35 }));
    panelA.position.x = -1.25;
    const panelB = panelA.clone(); panelB.position.x = 1.25;
    const dish = new THREE.Mesh(new THREE.ConeGeometry(0.34, 0.3, 32, 1, true), mat(PALETTE.nebula, { side: THREE.DoubleSide }));
    dish.position.set(0, 0.55, 0.2); dish.rotation.x = 0.5;
    const eye = new THREE.Mesh(new THREE.SphereGeometry(0.1, 18, 12),
      mat(PALETTE.accent, { emissive: PALETTE.accent, emissiveIntensity: 1.2 }));
    eye.position.set(0, 0, 0.58);
    sat.add(body, panelA, panelB, dish, eye);
    const orbit = new THREE.Mesh(new THREE.TorusGeometry(2.1, 0.035, 12, 96),
      mat(PALETTE.nebula));
    orbit.rotation.x = Math.PI / 2.4;
    group.add(sat, orbit);
    group.rotation.z = 0.08;
    return {
      group,
      radius: 2.2,
      tick(s) {
        orbit.rotation.z = s * 0.25;
        sat.rotation.y = Math.sin(s * 0.5) * 0.5;
        eye.material.emissiveIntensity = 0.7 + (Math.sin(s * 3) + 1) * 0.5;
      },
    };
  },
};

const motionQuery = window.matchMedia('(prefers-reduced-motion: reduce)');
const scenes = [];

function init(el) {
  const build = EMBLEMS[el.dataset.emblem];
  if (!build) return;

  const renderer = new THREE.WebGLRenderer({ antialias: true, alpha: true });
  renderer.toneMapping = THREE.ACESFilmicToneMapping;
  renderer.setClearColor(0x000000, 0);
  renderer.setPixelRatio(Math.min(window.devicePixelRatio, 2));
  el.appendChild(renderer.domElement);

  const scene = new THREE.Scene();
  scene.add(new THREE.AmbientLight(0x2A3350, 1.1));
  const key = new THREE.DirectionalLight(0xffffff, 3.2);
  key.position.set(3, 4, 5);
  const rim = new THREE.PointLight(PALETTE.nebula, 14, 20);
  rim.position.set(-4, -1, -3);
  scene.add(key, rim);

  const emblem = build(scene);
  scene.add(emblem.group);

  /* Each builder reports its true max extent as `radius`; back the camera up
     until that extent fits the frame with a 12% margin, whatever the shape.
     Guessing distances is how rings end up cropped into grey sticks. */
  const FOV = 38;
  const dist = (emblem.radius * 1.12) / Math.tan(THREE.MathUtils.degToRad(FOV / 2));
  const camera = new THREE.PerspectiveCamera(FOV, 1, 0.1, 100);
  camera.position.set(0, dist * 0.18, dist);
  camera.lookAt(0, -0.05, 0);

  const entry = { el, renderer, scene, camera, emblem, visible: false, dragging: false, userSpin: null };

  function resize() {
    const w = el.clientWidth || 1;
    const h = el.clientHeight || 1;
    renderer.setSize(w, h, false);
    renderer.domElement.style.width = '100%';
    renderer.domElement.style.height = '100%';
    camera.aspect = w / h;
    camera.updateProjectionMatrix();
  }
  resize();
  new ResizeObserver(resize).observe(el);

  /* Drag to spin. Taking hold cancels the automatic sway for this emblem. */
  let px = 0;
  el.addEventListener('pointerdown', (e) => {
    entry.dragging = true;
    entry.userSpin = entry.userSpin ?? emblem.group.rotation.y;
    px = e.clientX;
    el.setPointerCapture(e.pointerId);
  });
  el.addEventListener('pointermove', (e) => {
    if (!entry.dragging) return;
    entry.userSpin += (e.clientX - px) * 0.012;
    px = e.clientX;
  });
  el.addEventListener('pointerup', () => { entry.dragging = false; });
  el.addEventListener('pointercancel', () => { entry.dragging = false; });

  scenes.push(entry);
  entry.render = (s) => {
    if (!motionQuery.matches) emblem.tick(s);
    emblem.group.rotation.y = entry.userSpin ?? (motionQuery.matches ? 0 : Math.sin(s * 0.18) * 0.26);
    renderer.render(scene, camera);
  };
  entry.render(0); /* first paint even before the observer fires */
}

document.querySelectorAll('[data-emblem]').forEach(init);

/* One shared loop; only visible, motion-friendly scenes render. */
const io = new IntersectionObserver((entries) => {
  entries.forEach((e) => {
    const s = scenes.find((x) => x.el === e.target);
    if (s) s.visible = e.isIntersecting;
  });
}, { rootMargin: '60px' });
scenes.forEach((s) => io.observe(s.el));

function loop(t) {
  requestAnimationFrame(loop);
  if (motionQuery.matches) return; /* stills only; re-renders happen on change */
  const s = t / 1000;
  scenes.forEach((x) => { if (x.visible) x.render(s); });
}
requestAnimationFrame(loop);

if (motionQuery.addEventListener) {
  motionQuery.addEventListener('change', () => {
    scenes.forEach((x) => x.render(0)); /* settle to a clean still */
  });
}
