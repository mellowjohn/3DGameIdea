(() => {
  const arm = Armature.all[0];
  for (const a of Animation.all) {
    if (a.selected) {
    }
  }
  try { Animator.showDefaultPose(true); } catch (e) {}
  for (const b of ArmatureBone.all) {
    b.extend({rotation: [0, 0, 0]});
    if (b.preview_controller) b.preview_controller.updateTransform(b);
  }
  Canvas.updateAllBones();
  Canvas.scene.updateMatrixWorld(true);
  try { Animator.displayMeshDeformation(); } catch (e) {}

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

  function worldOf(el) {
    const wp = new THREE.Vector3();
    el.mesh.getWorldPosition(wp);
    return wp;
  }

  const handMeshes = Mesh.all.filter(m => map[m.name]);
  for (const m of handMeshes) {
    m.addTo(arm);
  }

  for (const b of ArmatureBone.all) {
    if (!/Hand|Thumb|Index|Middle|Ring|Pinky/.test(b.name)) continue;
    const keep = {};
    for (const [k, v] of Object.entries(b.vertex_weights || {})) {
      const pref = k.split(":")[0];
      const mesh = Mesh.all.find(x => x.uuid.slice(0, 6) === pref);
      if (mesh && !map[mesh.name]) keep[k] = v;
    }
    b.vertex_weights = keep;
  }

  const placed = [];
  for (const m of handMeshes) {
    const bone = byBone[map[m.name]];
    const bw = worldOf(bone);
    m.extend({origin: [bw.x, bw.y, bw.z], rotation: [0, 0, 0]});
    if (m.name.indexOf("WristSeam") >= 0) {
      const side = m.name[0] === "L" ? -1 : 1;
      m.extend({origin: [bw.x + side * (-1.364), bw.y, bw.z]});
    }
    if (m.preview_controller) m.preview_controller.updateTransform(m);
    for (const vk of Object.keys(m.vertices)) bone.setVertexWeight(m, vk, 1);
    placed.push({name: m.name, origin: m.origin.map(v => +v.toFixed(2)), bone: bone.name});
  }

  Canvas.updateAllBones();
  Canvas.scene.updateMatrixWorld(true);
  Preview.selected.render();

  let fingerWeighted = 0;
  for (const b of ArmatureBone.all) {
    if (!/Thumb|Index|Middle|Ring|Pinky/.test(b.name)) continue;
    fingerWeighted += Object.keys(b.vertex_weights || {}).length;
  }

  const fs = require("fs");
  const data = Codecs.project.compile({raw: true});
  const json = typeof data === "string" ? data : JSON.stringify(data);
  for (const p of [
    "C:/Users/johnr/Documents/GoodPlayerModel.bbmodel",
    "C:/Users/johnr/Documents/GoodPlayerModel_rigged.bbmodel",
    "C:/Users/johnr/Documents/3DGameIdea/tools/art/player/GoodPlayerModel_rigged.bbmodel"
  ]) fs.writeFileSync(p, json);

  return {
    fingerWeighted,
    placed: placed.slice(0, 6),
    LPalm: placed.find(p => p.name === "LPalm"),
    LIndex1: placed.find(p => p.name === "LIndex1"),
    bytes: json.length
  };
})()
