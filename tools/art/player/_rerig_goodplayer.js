(() => {
  function smoothstep(e0, e1, x) {
    if (e0 === e1) return x < e0 ? 0 : 1;
    const t = Math.max(0, Math.min(1, (x - e0) / (e1 - e0)));
    return t * t * (3 - 2 * t);
  }
  function dist(a, b) {
    const dx = a[0] - b[0], dy = a[1] - b[1], dz = a[2] - b[2];
    return Math.sqrt(dx * dx + dy * dy + dz * dz);
  }
  function worldOf(el) {
    const wp = new THREE.Vector3();
    el.mesh.getWorldPosition(wp);
    return [wp.x, wp.y, wp.z];
  }

  Modes.options.edit.select();
  Undo.initEdit({elements: [...ArmatureBone.all, ...Mesh.all], outliner: true, selection: true});

  const byMesh = {};
  for (const m of Mesh.all) byMesh[m.name] = m;
  const body = byMesh.BodyMesh;
  const arm = Armature.all[0];
  if (!body || !arm) return JSON.stringify({error: "missing body/armature"});

  for (const b of ArmatureBone.all) {
    b.extend({rotation: [0, 0, 0]});
    if (b.preview_controller) b.preview_controller.updateTransform(b);
  }
  Canvas.updateAllBones();
  Canvas.scene.updateMatrixWorld(true);

  const o = body.origin;
  const samples = [];
  for (const v of Object.values(body.vertices)) samples.push([v[0] + o[0], v[1] + o[1], v[2] + o[2]]);
  function avgBand(pred) {
    const g = samples.filter(pred);
    if (!g.length) return null;
    return [
      g.reduce((a, p) => a + p[0], 0) / g.length,
      g.reduce((a, p) => a + p[1], 0) / g.length,
      g.reduce((a, p) => a + p[2], 0) / g.length,
      g.length
    ];
  }
  const shoulderL = avgBand(p => p[0] > 4.8 && p[0] < 5.8 && p[1] > 27.5 && p[1] < 30.5);
  const elbowL = avgBand(p => Math.abs(p[0] - 12.95) < 0.4 && p[1] > 28);
  const wristL = avgBand(p => p[0] > 18.8 && p[1] > 28);
  const hipY = 16.0;
  const kneeL = avgBand(p => p[0] > 1.2 && p[0] < 3.5 && p[1] > 8.5 && p[1] < 10.5);
  const ankleL = avgBand(p => p[0] > 1.0 && p[0] < 3.5 && p[1] > 2.5 && p[1] < 4.5);
  const palmL = worldOf(byMesh.LPalm);
  const palmR = worldOf(byMesh.RPalm);

  const W = {
    Hips: [0, hipY, 0.5],
    Spine: [0, 20.0, 0.5],
    Chest: [0, 27.0, 0.5],
    Neck: [0, 29.5, 0.6],
    Head: [0, 32.0, 0.0],
    LeftUpperArm: [shoulderL ? shoulderL[0] : 5.2, shoulderL ? shoulderL[1] : 29.5, shoulderL ? shoulderL[2] : 0.5],
    LeftLowerArm: [elbowL ? elbowL[0] : 12.95, elbowL ? elbowL[1] : 29.43, elbowL ? elbowL[2] : 0.89],
    LeftHand: palmL,
    RightUpperArm: [-(shoulderL ? shoulderL[0] : 5.2), shoulderL ? shoulderL[1] : 29.5, shoulderL ? shoulderL[2] : 0.5],
    RightLowerArm: [-(elbowL ? elbowL[0] : 12.95), elbowL ? elbowL[1] : 29.43, elbowL ? elbowL[2] : 0.89],
    RightHand: palmR,
    LeftUpperLeg: [2.2, hipY - 0.5, 0.5],
    LeftLowerLeg: [2.2, kneeL ? kneeL[1] : 9.5, kneeL ? kneeL[2] : 0.5],
    LeftFoot: [2.2, ankleL ? ankleL[1] : 3.0, (ankleL ? ankleL[2] : 0.5) + 0.3],
    RightUpperLeg: [-2.2, hipY - 0.5, 0.5],
    RightLowerLeg: [-2.2, kneeL ? kneeL[1] : 9.5, kneeL ? kneeL[2] : 0.5],
    RightFoot: [-2.2, ankleL ? ankleL[1] : 3.0, (ankleL ? ankleL[2] : 0.5) + 0.3]
  };

  const by = {};
  for (const b of ArmatureBone.all) by[b.name] = b;

  for (const m of Mesh.all) {
    if (m.parent && m.parent instanceof ArmatureBone) m.addTo(arm);
  }

  function setLocalFromWorld(boneName, parentWorld) {
    const b = by[boneName];
    const w = W[boneName];
    b.extend({origin: [w[0] - parentWorld[0], w[1] - parentWorld[1], w[2] - parentWorld[2]], rotation: [0, 0, 0]});
    return w;
  }

  setLocalFromWorld("Hips", [0, 0, 0]);
  setLocalFromWorld("Spine", W.Hips);
  setLocalFromWorld("Chest", W.Spine);
  setLocalFromWorld("Neck", W.Chest);
  setLocalFromWorld("Head", W.Neck);
  setLocalFromWorld("LeftUpperArm", W.Chest);
  setLocalFromWorld("LeftLowerArm", W.LeftUpperArm);
  setLocalFromWorld("LeftHand", W.LeftLowerArm);
  setLocalFromWorld("RightUpperArm", W.Chest);
  setLocalFromWorld("RightLowerArm", W.RightUpperArm);
  setLocalFromWorld("RightHand", W.RightLowerArm);
  setLocalFromWorld("LeftUpperLeg", W.Hips);
  setLocalFromWorld("LeftLowerLeg", W.LeftUpperLeg);
  setLocalFromWorld("LeftFoot", W.LeftLowerLeg);
  setLocalFromWorld("RightUpperLeg", W.Hips);
  setLocalFromWorld("RightLowerLeg", W.RightUpperLeg);
  setLocalFromWorld("RightFoot", W.RightLowerLeg);

  function setLen(name, tipWorld) {
    const b = by[name];
    b.extend({length: Math.max(0.4, dist(W[name], tipWorld))});
  }
  setLen("Hips", W.Spine);
  setLen("Spine", W.Chest);
  setLen("Chest", W.Neck);
  setLen("Neck", W.Head);
  by.Head.extend({length: 4});
  setLen("LeftUpperArm", W.LeftLowerArm);
  setLen("LeftLowerArm", W.LeftHand);
  by.LeftHand.extend({length: 2.2});
  setLen("RightUpperArm", W.RightLowerArm);
  setLen("RightLowerArm", W.RightHand);
  by.RightHand.extend({length: 2.2});
  setLen("LeftUpperLeg", W.LeftLowerLeg);
  setLen("LeftLowerLeg", W.LeftFoot);
  by.LeftFoot.extend({length: 3});
  setLen("RightUpperLeg", W.RightLowerLeg);
  setLen("RightLowerLeg", W.RightFoot);
  by.RightFoot.extend({length: 3});

  for (const b of ArmatureBone.all) {
    if (b.preview_controller) b.preview_controller.updateTransform(b);
  }
  Canvas.updateAllBones();
  Canvas.scene.updateMatrixWorld(true);

  function snapFinger(boneName, meshName, childBoneName, tipMeshName) {
    const bone = by[boneName];
    const mesh = byMesh[meshName];
    if (!bone || !mesh) return;
    const parent = bone.parent;
    const pw2 = worldOf(parent);
    const mw = worldOf(mesh);
    bone.extend({origin: [mw[0] - pw2[0], mw[1] - pw2[1], mw[2] - pw2[2]], rotation: [0, 0, 0]});
    if (childBoneName && tipMeshName) {
      if (bone.preview_controller) bone.preview_controller.updateTransform(bone);
      Canvas.updateAllBones();
      Canvas.scene.updateMatrixWorld(true);
      const child = by[childBoneName];
      const tip = byMesh[tipMeshName];
      const bw = worldOf(bone);
      const tw = worldOf(tip);
      child.extend({origin: [tw[0] - bw[0], tw[1] - bw[1], tw[2] - bw[2]], rotation: [0, 0, 0]});
      bone.extend({length: Math.max(0.35, dist(mw, tw))});
      child.extend({length: 0.65});
    }
  }
  const digits = [
    ["LeftThumb1", "LThumb1", "LeftThumb2", "LThumb2"],
    ["LeftIndex1", "LIndex1", "LeftIndex2", "LIndex2"],
    ["LeftMiddle1", "LMiddle1", "LeftMiddle2", "LMiddle2"],
    ["LeftRing1", "LRing1", "LeftRing2", "LRing2"],
    ["LeftPinky1", "LPinky1", "LeftPinky2", "LPinky2"],
    ["RightThumb1", "RThumb1", "RightThumb2", "RThumb2"],
    ["RightIndex1", "RIndex1", "RightIndex2", "RIndex2"],
    ["RightMiddle1", "RMiddle1", "RightMiddle2", "RMiddle2"],
    ["RightRing1", "RRing1", "RightRing2", "RRing2"],
    ["RightPinky1", "RPinky1", "RightPinky2", "RPinky2"]
  ];
  for (const d of digits) snapFinger(d[0], d[1], d[2], d[3]);

  for (const b of ArmatureBone.all) {
    if (b.preview_controller) b.preview_controller.updateTransform(b);
  }
  Canvas.updateAllBones();
  Canvas.scene.updateMatrixWorld(true);

  const handMap = {
    LPalm: "LeftHand", LWristSeam: "LeftHand",
    LThumb1: "LeftThumb1", LThumb2: "LeftThumb2",
    LIndex1: "LeftIndex1", LIndex2: "LeftIndex2",
    LMiddle1: "LeftMiddle1", LMiddle2: "LeftMiddle2",
    LRing1: "LeftRing1", LRing2: "LeftRing2",
    LPinky1: "LeftPinky1", LPinky2: "LeftPinky2",
    RPalm: "RightHand", RWristSeam: "RightHand",
    RThumb1: "RightThumb1", RThumb2: "RightThumb2",
    RIndex1: "RightIndex1", RIndex2: "RightIndex2",
    RMiddle1: "RightMiddle1", RMiddle2: "RightMiddle2",
    RRing1: "RightRing1", RRing2: "RightRing2",
    RPinky1: "RightPinky1", RPinky2: "RightPinky2"
  };
  for (const [mn, bn] of Object.entries(handMap)) {
    const m = byMesh[mn];
    const b = by[bn];
    if (m && b) m.addTo(b);
  }
  for (const name of ["BodyMesh", "HeadMesh", "NeckMesh", "Nose", "BrowLeft", "BrowRight"]) {
    const m = byMesh[name];
    if (m) m.addTo(arm);
  }

  for (const b of ArmatureBone.all) b.vertex_weights = {};

  function classify(wx, wy) {
    const ax = Math.abs(wx), blend = 1.2;
    if (ax > 5.0 && wy >= 24.0) {
      const side = wx >= 0 ? "Left" : "Right";
      const shoulder = 6.0, elbow = 13.0, wrist = 19.15;
      if (ax < shoulder + blend) {
        const t = smoothstep(shoulder - blend, shoulder + blend, ax);
        return {Chest: 1 - t, [side + "UpperArm"]: t};
      }
      if (ax < elbow + blend) {
        const t = smoothstep(elbow - blend, elbow + blend, ax);
        if (ax < elbow - blend) return {[side + "UpperArm"]: 1};
        return {[side + "UpperArm"]: 1 - t, [side + "LowerArm"]: t};
      }
      const t = smoothstep(wrist - blend, wrist + blend, ax);
      if (ax < wrist - blend) return {[side + "LowerArm"]: 1};
      return {[side + "LowerArm"]: 1 - t, [side + "Hand"]: t};
    }
    if (ax < 6.5 && wy < 16.5) {
      const side = wx >= 0 ? "Left" : "Right";
      const hip = 15.5, knee = 9.0, ankle = 3.5;
      if (wy > hip - blend) {
        const t = smoothstep(hip - blend, hip + 0.5, wy);
        return {Hips: t, [side + "UpperLeg"]: 1 - t};
      }
      if (wy > knee - blend) {
        const t = smoothstep(knee - blend, knee + blend, wy);
        if (wy > knee + blend) return {[side + "UpperLeg"]: 1};
        return {[side + "UpperLeg"]: t, [side + "LowerLeg"]: 1 - t};
      }
      const t = smoothstep(ankle - blend, ankle + blend, wy);
      if (wy > ankle + blend) return {[side + "LowerLeg"]: 1};
      return {[side + "LowerLeg"]: t, [side + "Foot"]: 1 - t};
    }
    const hips_y = 16, spine_y = 20, chest_y = 26;
    if (wy < hips_y + blend) {
      const t = smoothstep(hips_y - blend, hips_y + blend, wy);
      if (wy < hips_y - blend) return {Hips: 1};
      return {Hips: 1 - t, Spine: t};
    }
    if (wy < spine_y + blend) {
      const t = smoothstep(spine_y - blend, spine_y + blend, wy);
      if (wy < spine_y - blend) return {Spine: 1};
      return {Spine: 1 - t, Chest: t};
    }
    if (wy < chest_y + blend) {
      const t = smoothstep(chest_y - blend, chest_y + blend, wy);
      if (wy < chest_y - blend) return {Chest: 1};
      return {Chest: 1 - t * 0.35, Neck: t * 0.35};
    }
    return {Chest: 0.25, Neck: 0.75};
  }

  let bodyAssigned = 0;
  for (const vkey of Object.keys(body.vertices)) {
    const v = body.vertices[vkey];
    const w = classify(v[0] + o[0], v[1] + o[1]);
    for (const [bn, wt] of Object.entries(w)) {
      if (wt <= 0.001 || !by[bn]) continue;
      by[bn].setVertexWeight(body, vkey, wt);
      bodyAssigned++;
    }
  }

  for (const name of ["HeadMesh", "Nose", "BrowLeft", "BrowRight"]) {
    const m = byMesh[name];
    if (!m) continue;
    for (const vk of Object.keys(m.vertices)) by.Head.setVertexWeight(m, vk, 1);
  }
  if (byMesh.NeckMesh) {
    for (const vk of Object.keys(byMesh.NeckMesh.vertices)) {
      by.Neck.setVertexWeight(byMesh.NeckMesh, vk, 0.7);
      by.Chest.setVertexWeight(byMesh.NeckMesh, vk, 0.3);
    }
  }
  for (const [mn, bn] of Object.entries(handMap)) {
    const m = byMesh[mn];
    if (!m || !by[bn]) continue;
    for (const vk of Object.keys(m.vertices)) by[bn].setVertexWeight(m, vk, 1);
  }

  for (const b of ArmatureBone.all) {
    b.visibility = true;
    b.extend({rotation: [0, 0, 0]});
    if (b.preview_controller) b.preview_controller.updateTransform(b);
  }
  try { Animator.showDefaultPose(false); } catch (e) {}
  Canvas.updateAllBones();
  Canvas.scene.updateMatrixWorld(true);
  try { Mesh.preview_controller.updateGeometry(body); } catch (e) {}
  try { Animator.displayMeshDeformation(); } catch (e) {}
  Preview.selected.render();

  let unweighted = 0;
  const unweightedMeshes = [];
  for (const m of Mesh.all) {
    let bad = 0;
    for (const vk of Object.keys(m.vertices)) {
      let sum = 0;
      for (const b of ArmatureBone.all) sum += b.getVertexWeight(m, vk);
      if (sum < 0.5) { unweighted++; bad++; }
    }
    if (bad) unweightedMeshes.push(m.name + ":" + bad);
  }

  Undo.finishEdit("Remake player bones + skinning");

  const fs = require("fs");
  const data = Codecs.project.compile({raw: true});
  const json = typeof data === "string" ? data : JSON.stringify(data);
  const paths = [
    "C:/Users/johnr/Documents/GoodPlayerModel.bbmodel",
    "C:/Users/johnr/Documents/GoodPlayerModel_rigged.bbmodel",
    "C:/Users/johnr/Documents/3DGameIdea/tools/art/player/GoodPlayerModel_rigged.bbmodel"
  ];
  for (const p of paths) fs.writeFileSync(p, json);

  return JSON.stringify({
    ok: true,
    boneCount: ArmatureBone.all.length,
    meshCount: Mesh.all.length,
    bodyAssigned,
    unweighted,
    unweightedMeshes,
    landmarks: {
      shoulderL, elbowL, wristL, kneeL, ankleL, palmL, palmR,
      LeftUpperArm: by.LeftUpperArm.origin,
      LeftLowerArm: by.LeftLowerArm.origin,
      LeftHand: by.LeftHand.origin
    },
    bytes: json.length,
    allRotZero: ArmatureBone.all.every(b => !b.rotation[0] && !b.rotation[1] && !b.rotation[2])
  }, null, 2);
})()
