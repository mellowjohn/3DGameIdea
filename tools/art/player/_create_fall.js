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

  const existing = Animation.all.find(a => a.name === "Fall");
  if (existing) existing.remove(false);

  const anim = new Animation({
    name: "Fall",
    loop: "loop",
    length: 1.0,
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
    [0.0, [0, 1.8, 0.15]],
    [0.5, [0, 2.6, 0.2]],
    [1.0, [0, 1.8, 0.15]]
  ]);
  setRot("Hips", [
    [0.0, [-8, 0, 0]],
    [0.5, [-11, 0, 0]],
    [1.0, [-8, 0, 0]]
  ]);
  setRot("Spine", [
    [0.0, [-6, 0, 0]],
    [0.5, [-9, 0, 0]],
    [1.0, [-6, 0, 0]]
  ]);
  setRot("Chest", [
    [0.0, [-10, 0, 0]],
    [0.5, [-16, 0, 0]],
    [1.0, [-10, 0, 0]]
  ]);
  setRot("Neck", [
    [0.0, [10, 0, 0]],
    [0.5, [12, 0, 0]],
    [1.0, [10, 0, 0]]
  ]);
  setRot("Head", [
    [0.0, [4, 0, 0]],
    [0.5, [7, 0, 0]],
    [1.0, [4, 0, 0]]
  ]);

  setRot("LeftUpperArm", [
    [0.0, [5, 18, -48]],
    [0.5, [10, 12, -40]],
    [1.0, [5, 18, -48]]
  ]);
  setRot("LeftLowerArm", [
    [0.0, [6, 88, -8]],
    [0.5, [2, 78, -4]],
    [1.0, [6, 88, -8]]
  ]);
  setRot("LeftHand", [
    [0.0, [5, 5, -15]],
    [0.5, [8, 0, -20]],
    [1.0, [5, 5, -15]]
  ]);

  setRot("RightUpperArm", [
    [0.0, [5, -18, 48]],
    [0.5, [10, -12, 40]],
    [1.0, [5, -18, 48]]
  ]);
  setRot("RightLowerArm", [
    [0.0, [6, -88, 8]],
    [0.5, [2, -78, 4]],
    [1.0, [6, -88, 8]]
  ]);
  setRot("RightHand", [
    [0.0, [5, -5, 15]],
    [0.5, [8, 0, 20]],
    [1.0, [5, -5, 15]]
  ]);

  setRot("LeftUpperLeg", [
    [0.0, [52, -6, -5]],
    [0.5, [46, -4, -4]],
    [1.0, [52, -6, -5]]
  ]);
  setRot("LeftLowerLeg", [
    [0.0, [-92, 0, 0]],
    [0.5, [-84, 0, 0]],
    [1.0, [-92, 0, 0]]
  ]);
  setRot("LeftFoot", [
    [0.0, [22, 0, 0]],
    [0.5, [16, 0, 0]],
    [1.0, [22, 0, 0]]
  ]);

  setRot("RightUpperLeg", [
    [0.0, [46, 6, 5]],
    [0.5, [52, 5, 4]],
    [1.0, [46, 6, 5]]
  ]);
  setRot("RightLowerLeg", [
    [0.0, [-84, 0, 0]],
    [0.5, [-92, 0, 0]],
    [1.0, [-84, 0, 0]]
  ]);
  setRot("RightFoot", [
    [0.0, [16, 0, 0]],
    [0.5, [22, 0, 0]],
    [1.0, [16, 0, 0]]
  ]);

  const fists = {
    LeftThumb1: [20, -30, 12], LeftThumb2: [8, 0, 30],
    LeftIndex1: [0, 0, 40], LeftIndex2: [0, 0, 45],
    LeftMiddle1: [0, 0, 42], LeftMiddle2: [0, 0, 48],
    LeftRing1: [0, 0, 38], LeftRing2: [0, 0, 44],
    LeftPinky1: [0, 0, 35], LeftPinky2: [0, 0, 40],
    RightThumb1: [20, 30, -12], RightThumb2: [8, 0, -30],
    RightIndex1: [0, 0, -40], RightIndex2: [0, 0, -45],
    RightMiddle1: [0, 0, -42], RightMiddle2: [0, 0, -48],
    RightRing1: [0, 0, -38], RightRing2: [0, 0, -44],
    RightPinky1: [0, 0, -35], RightPinky2: [0, 0, -40]
  };
  for (const [name, r] of Object.entries(fists)) {
    setRot(name, [[0.0, r], [1.0, r]]);
  }

  function world(n) {
    const b = ArmatureBone.all.find(x => x.name === n);
    const wp = new THREE.Vector3();
    b.mesh.getWorldPosition(wp);
    return wp.toArray().map(v => +v.toFixed(2));
  }
  function euler(n) {
    const b = ArmatureBone.all.find(x => x.name === n);
    const e = new THREE.Euler().setFromQuaternion(b.mesh.quaternion.clone(), "XYZ");
    return [e.x, e.y, e.z].map(v => +(v * 180 / Math.PI).toFixed(1));
  }

  Timeline.setTime(0.25);
  Animator.preview(true);
  Canvas.updateAllBones();
  try { Animator.displayMeshDeformation(); } catch (e) {}
  Preview.selected.render();

  const sample = {
    hips: euler("Hips"),
    chest: euler("Chest"),
    LFoot: world("LeftFoot"),
    RFoot: world("RightFoot"),
    LHand: world("LeftHand"),
    RHand: world("RightHand"),
    LElbow: world("LeftLowerArm"),
    RElbow: world("RightLowerArm"),
    L_dy: null,
    R_dy: null
  };
  sample.L_dy = +(sample.LHand[1] - sample.LElbow[1]).toFixed(2);
  sample.R_dy = +(sample.RHand[1] - sample.RElbow[1]).toFixed(2);
  sample.L_dz = +(sample.LHand[2] - sample.LElbow[2]).toFixed(2);
  sample.R_dz = +(sample.RHand[2] - sample.RElbow[2]).toFixed(2);

  const fs = require("fs");
  const data = Codecs.project.compile({ raw: true });
  const json = typeof data === "string" ? data : JSON.stringify(data);
  for (const p of [
    "C:/Users/johnr/Documents/GoodPlayerModel.bbmodel",
    "C:/Users/johnr/Documents/GoodPlayerModel_rigged.bbmodel",
    "C:/Users/johnr/Documents/3DGameIdea/tools/art/player/GoodPlayerModel_rigged.bbmodel"
  ]) fs.writeFileSync(p, json);

  return {
    name: anim.name,
    length: anim.length,
    loop: anim.loop,
    sample,
    anims: Animation.all.map(a => a.name),
    bytes: json.length
  };
})()
