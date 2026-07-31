(() => {
  function clearChannel(animator, channel) {
    const arr = animator[channel] || [];
    while (arr.length) arr[arr.length - 1].remove(false);
  }
  function addRot(animator, time, xyz) {
    return animator.addKeyframe({
      channel: "rotation",
      time: time,
      interpolation: "catmullrom",
      data_points: [{ x: xyz[0], y: xyz[1], z: xyz[2] }]
    });
  }
  function addPos(animator, time, xyz) {
    return animator.addKeyframe({
      channel: "position",
      time: time,
      interpolation: "catmullrom",
      data_points: [{ x: xyz[0], y: xyz[1], z: xyz[2] }]
    });
  }

  const existing = Animation.all.find(a => a.name === "Run");
  if (existing) existing.remove(false);

  const anim = new Animation({
    name: "Run",
    loop: "loop",
    length: 0.8,
    snapping: 24
  }).add(false);
  anim.select();

  const by = {};
  for (const b of ArmatureBone.all) by[b.name] = anim.getBoneAnimator(b);

  function setRot(name, keys) {
    const an = by[name];
    clearChannel(an, "rotation");
    for (const [t, r] of keys) addRot(an, t, r);
  }
  function setPos(name, keys) {
    const an = by[name];
    clearChannel(an, "position");
    for (const [t, p] of keys) addPos(an, t, p);
  }

  setPos("Hips", [
    [0.0, [0, -1.0, 0.15]],
    [0.2, [0, 0.35, 0.2]],
    [0.3, [0, 1.2, 0.25]],
    [0.4, [0, -0.15, 0.15]],
    [0.5, [0, -1.0, 0.15]],
    [0.6, [0, 0.35, 0.2]],
    [0.7, [0, 1.2, 0.25]],
    [0.8, [0, -1.0, 0.15]]
  ]);
  setRot("Hips", [
    [0.0, [-10, 4, -2]],
    [0.2, [-6, 0, 0]],
    [0.3, [-4, -4, 0]],
    [0.4, [-8, -5, 0]],
    [0.5, [-10, -4, 2]],
    [0.6, [-6, 0, 0]],
    [0.7, [-4, 4, 0]],
    [0.8, [-10, 4, -2]]
  ]);
  setRot("Spine", [
    [0.0, [-8, -2, -1]],
    [0.3, [-6, 2, 0]],
    [0.4, [-7, 3, 0]],
    [0.5, [-8, 2, 1]],
    [0.7, [-6, -2, 0]],
    [0.8, [-8, -2, -1]]
  ]);
  setRot("Chest", [
    [0.0, [-12, -5, -2]],
    [0.3, [-9, 4, 0]],
    [0.4, [-11, 7, 0]],
    [0.5, [-12, 5, 2]],
    [0.7, [-9, -4, 0]],
    [0.8, [-12, -5, -2]]
  ]);
  setRot("Neck", [
    [0.0, [16, 5, 0]],
    [0.2, [17, 0, 0]],
    [0.4, [16, -5, 0]],
    [0.6, [17, 0, 0]],
    [0.8, [16, 5, 0]]
  ]);
  setRot("Head", [
    [0.0, [3, -3, 1]],
    [0.4, [3, 3, -1]],
    [0.8, [3, -3, 1]]
  ]);

  setRot("LeftUpperLeg", [
    [0.0, [-32, 5, -3]],
    [0.1, [-22, 3, -2]],
    [0.2, [15, 0, 0]],
    [0.3, [48, 3, -2]],
    [0.4, [42, 5, -3]],
    [0.5, [28, 4, -2]],
    [0.6, [8, 2, -1]],
    [0.7, [-18, 0, 0]],
    [0.8, [-32, 5, -3]]
  ]);
  setRot("LeftLowerLeg", [
    [0.0, [-72, 0, 0]],
    [0.1, [-85, 0, 0]],
    [0.2, [-95, 0, 0]],
    [0.3, [-70, 0, 0]],
    [0.4, [-18, 0, 0]],
    [0.5, [-48, 0, 0]],
    [0.6, [-12, 0, 0]],
    [0.7, [-55, 0, 0]],
    [0.8, [-72, 0, 0]]
  ]);
  setRot("LeftFoot", [
    [0.0, [25, 0, 0]],
    [0.2, [10, 0, 0]],
    [0.3, [-5, 0, 0]],
    [0.4, [-12, 0, 0]],
    [0.5, [-28, 0, 0]],
    [0.6, [5, 0, 0]],
    [0.7, [15, 0, 0]],
    [0.8, [25, 0, 0]]
  ]);
  setRot("RightUpperLeg", [
    [0.0, [42, -5, 3]],
    [0.1, [28, -4, 2]],
    [0.2, [8, -2, 1]],
    [0.3, [-18, 0, 0]],
    [0.4, [-32, 4, -2]],
    [0.5, [-22, 3, -2]],
    [0.6, [15, 0, 0]],
    [0.7, [48, -3, 2]],
    [0.8, [42, -5, 3]]
  ]);
  setRot("RightLowerLeg", [
    [0.0, [-18, 0, 0]],
    [0.1, [-48, 0, 0]],
    [0.2, [-12, 0, 0]],
    [0.3, [-55, 0, 0]],
    [0.4, [-72, 0, 0]],
    [0.5, [-85, 0, 0]],
    [0.6, [-95, 0, 0]],
    [0.7, [-70, 0, 0]],
    [0.8, [-18, 0, 0]]
  ]);
  setRot("RightFoot", [
    [0.0, [-12, 0, 0]],
    [0.1, [-28, 0, 0]],
    [0.2, [5, 0, 0]],
    [0.3, [15, 0, 0]],
    [0.4, [25, 0, 0]],
    [0.6, [10, 0, 0]],
    [0.7, [-5, 0, 0]],
    [0.8, [-12, 0, 0]]
  ]);

  setRot("LeftUpperArm", [
    [0.0, [-28, 22, -78]],
    [0.2, [-4, 6, -74]],
    [0.4, [38, -28, -62]],
    [0.6, [8, -8, -72]],
    [0.8, [-28, 22, -78]]
  ]);
  setRot("LeftLowerArm", [
    [0.0, [-82, 2, -8]],
    [0.2, [-64, 4, -14]],
    [0.4, [-42, 8, -22]],
    [0.6, [-58, 5, -16]],
    [0.8, [-82, 2, -8]]
  ]);
  setRot("LeftHand", [
    [0.0, [12, -6, 8]],
    [0.4, [8, 6, 4]],
    [0.8, [12, -6, 8]]
  ]);

  setRot("RightUpperArm", [
    [0.0, [38, 28, 62]],
    [0.2, [8, 8, 72]],
    [0.4, [-28, -22, 78]],
    [0.6, [-4, -6, 74]],
    [0.8, [38, 28, 62]]
  ]);
  setRot("RightLowerArm", [
    [0.0, [-42, -8, 22]],
    [0.2, [-58, -5, 16]],
    [0.4, [-82, -2, 8]],
    [0.6, [-64, -4, 14]],
    [0.8, [-42, -8, 22]]
  ]);
  setRot("RightHand", [
    [0.0, [8, -6, -4]],
    [0.4, [12, 6, -8]],
    [0.8, [8, -6, -4]]
  ]);

  const fists = {
    LeftThumb1: [25, -35, 15], LeftThumb2: [10, 0, 40],
    LeftIndex1: [0, 0, 55], LeftIndex2: [0, 0, 63],
    LeftMiddle1: [0, 0, 58], LeftMiddle2: [0, 0, 66],
    LeftRing1: [0, 0, 52], LeftRing2: [0, 0, 60],
    LeftPinky1: [0, 0, 50], LeftPinky2: [0, 0, 57],
    RightThumb1: [25, 35, -15], RightThumb2: [10, 0, -40],
    RightIndex1: [0, 0, -55], RightIndex2: [0, 0, -63],
    RightMiddle1: [0, 0, -58], RightMiddle2: [0, 0, -66],
    RightRing1: [0, 0, -52], RightRing2: [0, 0, -60],
    RightPinky1: [0, 0, -50], RightPinky2: [0, 0, -57]
  };
  for (const [name, r] of Object.entries(fists)) {
    setRot(name, [[0.0, r], [0.8, r]]);
  }

  function world(n) {
    const b = ArmatureBone.all.find(x => x.name === n);
    const wp = new THREE.Vector3();
    b.mesh.getWorldPosition(wp);
    return { y: +wp.y.toFixed(2), z: +wp.z.toFixed(2) };
  }
  function euler(n) {
    const b = ArmatureBone.all.find(x => x.name === n);
    const e = new THREE.Euler().setFromQuaternion(b.mesh.quaternion.clone(), "XYZ");
    return [e.x, e.y, e.z].map(v => +(v * 180 / Math.PI).toFixed(1));
  }

  const samples = [];
  for (const t of [0, 0.4]) {
    Timeline.setTime(t);
    Animator.preview(true);
    Canvas.updateAllBones();
    try { Animator.displayMeshDeformation(); } catch (e) {}
    samples.push({
      t,
      LFoot: world("LeftFoot"),
      RFoot: world("RightFoot"),
      LHand: world("LeftHand"),
      RHand: world("RightHand"),
      hips: euler("Hips"),
      chest: euler("Chest"),
      LUA: euler("LeftUpperArm"),
      RUA: euler("RightUpperArm")
    });
  }
  Preview.selected.render();
  return { name: anim.name, length: anim.length, samples };
})()
