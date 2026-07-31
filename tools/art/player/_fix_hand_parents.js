(() => {
  const arm = Armature.all[0];
  const handMeshes = Mesh.all.filter(m => /^(L|R)(Palm|WristSeam|Thumb|Index|Middle|Ring|Pinky)/.test(m.name));

  const worlds = {};
  for (const m of handMeshes) {
    const wp = new THREE.Vector3();
    m.mesh.getWorldPosition(wp);
    const wq = new THREE.Quaternion();
    m.mesh.getWorldQuaternion(wq);
    worlds[m.uuid] = {pos: wp.clone(), quat: wq.clone()};
  }

  const map = {
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
  const byBone = {};
  for (const b of ArmatureBone.all) byBone[b.name] = b;

  for (const b of ArmatureBone.all) {
    if (!/Hand|Thumb|Index|Middle|Ring|Pinky/.test(b.name)) continue;
    const keep = {};
    for (const [k, v] of Object.entries(b.vertex_weights || {})) {
      const pref = k.split(":")[0];
      const mesh = Mesh.all.find(m => m.uuid.slice(0, 6) === pref);
      if (mesh && handMeshes.indexOf(mesh) < 0) keep[k] = v;
    }
    b.vertex_weights = keep;
  }

  for (const m of handMeshes) {
    const bn = map[m.name];
    const bone = byBone[bn];
    if (!bone) continue;
    for (const vk of Object.keys(m.vertices)) bone.setVertexWeight(m, vk, 1);
  }

  for (const m of handMeshes) {
    const w = worlds[m.uuid];
    m.addTo(arm);
    Canvas.updateAllBones();
    Canvas.scene.updateMatrixWorld(true);
    const parentInv = new THREE.Matrix4().copy(arm.mesh.matrixWorld).invert();
    const mat = new THREE.Matrix4().compose(w.pos, w.quat, new THREE.Vector3(1, 1, 1));
    mat.premultiply(parentInv);
    const pos = new THREE.Vector3();
    const quat = new THREE.Quaternion();
    const scl = new THREE.Vector3();
    mat.decompose(pos, quat, scl);
    const e = new THREE.Euler().setFromQuaternion(quat, "XYZ");
    m.extend({
      origin: [pos.x, pos.y, pos.z],
      rotation: [e.x * 180 / Math.PI, e.y * 180 / Math.PI, e.z * 180 / Math.PI]
    });
    if (m.preview_controller) m.preview_controller.updateTransform(m);
  }
  Canvas.updateView();

  let fingerWeighted = 0;
  for (const b of ArmatureBone.all) {
    if (!/Thumb|Index|Middle|Ring|Pinky/.test(b.name)) continue;
    fingerWeighted += Object.keys(b.vertex_weights || {}).length;
  }
  const after = handMeshes.map(m => ({
    name: m.name,
    parent: m.parent && m.parent.name,
    origin: m.origin.map(v => +v.toFixed(2)),
    verts: Object.keys(m.vertices).length
  }));
  return {handCount: handMeshes.length, fingerWeighted, after};
})()
