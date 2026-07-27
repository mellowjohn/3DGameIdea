"""Generate aligned player HUD + dialogue UI canvases (1920x1080 safe margin)."""
from __future__ import annotations

import json
from pathlib import Path

SAFE = 40.0
DW, DH = 1920.0, 1080.0
ROOT = Path(__file__).resolve().parents[1]
UI = ROOT / "samples" / "open-world-rpg" / "assets" / "ui"


def panel(id, anchor, offset, size, color, image=None, mode=None, visible=None):
    w = {
        "id": id,
        "type": "panel",
        "anchor": anchor,
        "offset": [float(offset[0]), float(offset[1])],
        "size": [float(size[0]), float(size[1])],
        "color": color,
    }
    if image:
        w["image"] = image
        w["imageMode"] = mode or "contain"
    if visible is not None:
        w["visible"] = visible
    return w


def text(
    id,
    anchor,
    offset,
    size,
    bind,
    color,
    font=16,
    align="left",
    valign="middle",
    text_default="",
    visible=None,
):
    w = {
        "id": id,
        "type": "text",
        "anchor": anchor,
        "offset": [float(offset[0]), float(offset[1])],
        "size": [float(size[0]), float(size[1])],
        "bind": bind,
        "text": text_default,
        "textAlign": align,
        "textVAlign": valign,
        "color": color,
        "fontSize": float(font),
    }
    if visible is not None:
        w["visible"] = visible
    return w


def bar(id, anchor, offset, size, bind, max_bind, color):
    return {
        "id": id,
        "type": "bar",
        "anchor": anchor,
        "offset": [float(offset[0]), float(offset[1])],
        "size": [float(size[0]), float(size[1])],
        "bind": bind,
        "maxBind": max_bind,
        "color": color,
    }


def build_player():
    face = 160.0
    face_x, face_y = SAFE, SAFE
    vitals_x = face_x + face + 24.0
    bar_w = 340.0
    slot = 64.0
    gap = 12.0
    hotbar_w = 6 * slot + 5 * gap
    hotbar_x = (DW - hotbar_w) / 2.0
    hotbar_y = SAFE
    mm = 168.0
    mm_inset = 22.0
    feed = mm - 2 * mm_inset
    dot = 12.0
    mm_ox, mm_oy = SAFE, SAFE
    feed_ox = mm_ox + mm_inset
    feed_oy = mm_oy + mm_inset
    dot_ox = feed_ox + (feed - dot) / 2.0
    dot_oy = feed_oy + (feed - dot) / 2.0

    widgets = [
        panel("hud_face_mid", "bottom_left", (face_x + 16, face_y + 16), (face - 32, face - 32), [58, 52, 44, 255]),
        panel(
            "hud_face_viewport",
            "bottom_left",
            (face_x + 28, face_y + 28),
            (face - 56, face - 56),
            [92, 78, 58, 255],
        ),
        panel(
            "hud_face_outer",
            "bottom_left",
            (face_x, face_y),
            (face, face),
            [42, 40, 38, 255],
            "assets/ui/hud/hud-portrait-ring-hollow.png",
            "contain",
        ),
        text(
            "hud_face_label",
            "bottom_left",
            (face_x + 28, face_y + 28),
            (face - 56, face - 56),
            "hud.faceLabel",
            [201, 184, 150, 255],
            16,
            "center",
            "middle",
            "FACE",
        ),
    ]

    vitals_h = 138.0
    y = face_y + (face - vitals_h) / 2.0
    widgets += [
        text("player_name", "bottom_left", (vitals_x, y + 110), (bar_w, 26), "player.name", [241, 238, 232, 255], 22),
        text(
            "player_health_label",
            "bottom_left",
            (vitals_x, y + 88),
            (120, 16),
            "hud.healthLabel",
            [155, 163, 167, 255],
            13,
            text_default="Health",
        ),
        panel(
            "player_health_frame",
            "bottom_left",
            (vitals_x, y + 56),
            (bar_w, 28),
            [30, 28, 24, 255],
            "assets/ui/hud/hud-resource-bar.png",
            "stretch",
        ),
        bar(
            "player_health",
            "bottom_left",
            (vitals_x + 18, y + 62),
            (bar_w - 36, 16),
            "player.health",
            "player.healthMax",
            [139, 46, 46, 255],
        ),
        text(
            "player_health_text",
            "bottom_left",
            (vitals_x + bar_w - 100, y + 60),
            (100, 20),
            "player.healthText",
            [155, 163, 167, 255],
            13,
            "right",
        ),
        text(
            "player_resource_label",
            "bottom_left",
            (vitals_x, y + 32),
            (120, 16),
            "player.resourceLabel",
            [155, 163, 167, 255],
            13,
        ),
        panel(
            "player_resource_frame",
            "bottom_left",
            (vitals_x, y),
            (bar_w, 28),
            [30, 28, 24, 255],
            "assets/ui/hud/hud-resource-bar.png",
            "stretch",
        ),
        bar(
            "player_resource",
            "bottom_left",
            (vitals_x + 18, y + 6),
            (bar_w - 36, 16),
            "player.resource",
            "player.resourceMax",
            [196, 162, 74, 255],
        ),
        text(
            "player_resource_text",
            "bottom_left",
            (vitals_x + bar_w - 100, y + 4),
            (100, 20),
            "player.resourceText",
            [155, 163, 167, 255],
            13,
            "right",
        ),
    ]

    icons = [
        "assets/ui/hud/hud-icon-sword.png",
        "assets/ui/hud/hud-icon-shield.png",
        "assets/ui/hud/hud-icon-sprint.png",
        None,
        None,
        None,
    ]
    for i in range(6):
        sx = hotbar_x + i * (slot + gap)
        sid = i + 1
        widgets.append(
            panel(
                f"hud_hotbar_{sid}",
                "bottom_left",
                (sx, hotbar_y + 20),
                (slot, slot),
                [30, 28, 24, 240],
                "assets/ui/hud/hud-ability-slot.png",
                "contain",
            )
        )
        if icons[i]:
            inset = 14.0
            widgets.append(
                panel(
                    f"hud_hotbar_{sid}_icon",
                    "bottom_left",
                    (sx + inset, hotbar_y + 20 + inset),
                    (slot - 2 * inset, slot - 2 * inset),
                    [255, 255, 255, 255],
                    icons[i],
                    "contain",
                )
            )
        widgets.append(
            text(
                f"hud_hotbar_{sid}_key",
                "bottom_left",
                (sx, hotbar_y),
                (slot, 18),
                f"hud.hotbar.{sid}",
                [213, 185, 120, 255],
                14,
                "center",
                "middle",
                str(sid),
            )
        )

    widgets += [
        panel("hud_minimap_feed", "top_right", (feed_ox, feed_oy), (feed, feed), [58, 64, 40, 255]),
        panel(
            "hud_minimap_outer",
            "top_right",
            (mm_ox, mm_oy),
            (mm, mm),
            [42, 40, 38, 255],
            "assets/ui/hud/hud-minimap-frame.png",
            "contain",
        ),
        panel("hud_minimap_dot", "top_right", (dot_ox, dot_oy), (dot, dot), [213, 185, 120, 255]),
        panel(
            "quest_objective_panel",
            "top_left",
            (SAFE, SAFE),
            (360, 84),
            [45, 41, 35, 220],
            "assets/ui/hud/hud-quest-panel.png",
            "stretch",
        ),
        text(
            "quest_objective_eyebrow",
            "top_left",
            (SAFE + 20, SAFE + 14),
            (120, 14),
            "hud.questEyebrow",
            [213, 185, 120, 255],
            11,
            text_default="ACTIVE",
        ),
        text(
            "quest_objective_text",
            "top_left",
            (SAFE + 20, SAFE + 34),
            (320, 36),
            "quest.objectiveText",
            [241, 238, 232, 255],
            15,
        ),
    ]

    return {
        "schemaVersion": 1,
        "id": "player_hud",
        "designResolution": [DW, DH],
        "scaleMode": "letterbox",
        "widgets": widgets,
    }


def build_dialogue():
    panel_w = 1248.0
    panel_h = 500.0
    panel_x = (DW - panel_w) / 2.0
    panel_y = 36.0
    pad = 24.0
    iron = (panel_x, panel_y, panel_w, panel_h)
    bronze = (panel_x + 8, panel_y + 8, panel_w - 16, panel_h - 16)
    parch = (panel_x + 16, panel_y + 16, panel_w - 32, panel_h - 32)
    content_l = parch[0] + pad
    content_r = parch[0] + parch[2] - pad
    content_w = content_r - content_l
    portrait = 96.0
    port_y = parch[1] + parch[3] - pad - portrait
    port_x = content_l
    speaker_x = port_x + portrait + 16
    speaker_w = content_w - portrait - 16

    row_h = 52.0
    row_gap = 10.0
    bottom_content = parch[1] + 20
    choice_ys = [bottom_content + i * (row_h + row_gap) for i in range(4)]
    choice_y_by_slot = list(reversed(choice_ys))
    prompt_h = 52.0
    prompt_y = choice_y_by_slot[0] + row_h + 12
    # Keep prompt fully below the portrait well.
    if prompt_y + prompt_h > port_y - 8:
        prompt_y = max(choice_y_by_slot[0] + row_h + 8, port_y - 8 - prompt_h)
    body_y = parch[1] + 72
    body_h = max(120.0, port_y - 16 - body_y)
    continue_w, continue_h = 200.0, 40.0
    continue_x = content_r - continue_w
    continue_y = parch[1] + 20

    key = 32.0
    key_inset = 12.0
    tone_w, standing_w = 100.0, 168.0
    chip_gap = 12.0
    edge = 16.0

    dw = [
        panel("dialogue_frame_outer", "bottom_left", (iron[0], iron[1]), (iron[2], iron[3]), [42, 40, 38, 255]),
        panel("dialogue_frame_mid", "bottom_left", (bronze[0], bronze[1]), (bronze[2], bronze[3]), [58, 52, 44, 255]),
        panel("dialogue_panel", "bottom_left", (parch[0], parch[1]), (parch[2], parch[3]), [201, 184, 150, 245]),
        panel(
            "dialogue_accent_bar",
            "bottom_left",
            (parch[0], parch[1] + parch[3] - 6),
            (parch[2], 6),
            [213, 185, 120, 255],
        ),
        panel(
            "dialogue_portrait_mid",
            "bottom_left",
            (port_x + 10, port_y + 10),
            (portrait - 20, portrait - 20),
            [58, 52, 44, 255],
        ),
        panel(
            "dialogue_portrait_viewport",
            "bottom_left",
            (port_x + 18, port_y + 18),
            (portrait - 36, portrait - 36),
            [92, 78, 58, 255],
        ),
        panel(
            "dialogue_portrait_outer",
            "bottom_left",
            (port_x, port_y),
            (portrait, portrait),
            [42, 40, 38, 255],
            "assets/ui/dialogue/dialogue-portrait-ring.png",
            "contain",
        ),
        text(
            "dialogue_portrait_initials",
            "bottom_left",
            (port_x + 18, port_y + 18),
            (portrait - 36, portrait - 36),
            "dialogue.portrait",
            [201, 184, 150, 255],
            20,
            "center",
            "middle",
            "?",
        ),
        text(
            "dialogue_speaker",
            "bottom_left",
            (speaker_x, port_y + portrait - 36),
            (speaker_w, 28),
            "dialogue.speaker",
            [72, 62, 48, 255],
            22,
            text_default="Traveler",
        ),
        text(
            "dialogue_role",
            "bottom_left",
            (speaker_x, port_y + portrait - 58),
            (speaker_w, 20),
            "dialogue.role",
            [96, 84, 68, 255],
            13,
        ),
        text(
            "dialogue_body",
            "bottom_left",
            (content_l, body_y),
            (content_w, body_h),
            "dialogue.body",
            [72, 62, 48, 255],
            20,
            "left",
            "top",
            "The path ahead forks. One leads to the village, the other into the mist.",
        ),
        {
            "id": "dialogue_continue",
            "type": "button",
            "anchor": "bottom_left",
            "offset": [continue_x, continue_y],
            "size": [continue_w, continue_h],
            "bind": "dialogue.continue",
            "text": "Continue",
            "textAlign": "center",
            "textVAlign": "middle",
            "fontSize": 17.0,
            "color": [213, 185, 120, 255],
            "image": "assets/ui/dialogue/dialogue-continue-button.png",
            "imageMode": "contain",
        },
        panel(
            "dialogue_prompt_panel",
            "bottom_left",
            (content_l, prompt_y),
            (content_w, prompt_h),
            [184, 160, 120, 255],
            "assets/ui/dialogue/dialogue-prompt-strip.png",
            "stretch",
            visible=False,
        ),
        text(
            "dialogue_prompt_label",
            "bottom_left",
            (content_l + 20, prompt_y + prompt_h - 22),
            (220, 14),
            "dialogue.prompt_label",
            [96, 84, 68, 255],
            10,
            text_default="LAST LINE",
            visible=False,
        ),
        text(
            "dialogue_prompt",
            "bottom_left",
            (content_l + 20, prompt_y + 8),
            (content_w - 40, 28),
            "dialogue.prompt",
            [72, 62, 48, 255],
            14,
            visible=False,
        ),
    ]

    for i in range(4):
        slot = i + 1
        ry = choice_y_by_slot[i]
        key_x = content_l + key_inset
        key_y = ry + (row_h - key) / 2.0
        standing_x = content_l + content_w - edge - standing_w
        tone_x = standing_x - chip_gap - tone_w
        chip_y = ry + (row_h - 22) / 2.0
        dw.append(
            {
                "id": f"dialogue_choice_{slot}",
                "type": "button",
                "anchor": "bottom_left",
                "offset": [content_l, ry],
                "size": [content_w, row_h],
                "bind": f"dialogue.choice_{slot}",
                "text": "",
                "textAlign": "left",
                "textVAlign": "middle",
                "fontSize": 17.0,
                "color": [232, 220, 198, 255],
                "visible": False,
            }
        )
        dw.append(
            panel(
                f"dialogue_choice_{slot}_key",
                "bottom_left",
                (key_x, key_y),
                (key, key),
                [30, 28, 24, 255],
                "assets/ui/dialogue/dialogue-key-badge.png",
                "contain",
                visible=False,
            )
        )
        dw.append(
            text(
                f"dialogue_choice_{slot}_key_text",
                "bottom_left",
                (key_x, key_y),
                (key, key),
                f"dialogue.choice_{slot}_key",
                [213, 185, 120, 255],
                14,
                "center",
                "middle",
                str(slot),
                visible=False,
            )
        )
        dw.append(
            panel(
                f"dialogue_choice_{slot}_tone",
                "bottom_left",
                (tone_x, chip_y),
                (tone_w, 22),
                [60, 52, 38, 255],
                visible=False,
            )
        )
        dw.append(
            text(
                f"dialogue_choice_{slot}_tone_text",
                "bottom_left",
                (tone_x, chip_y),
                (tone_w, 22),
                f"dialogue.choice_{slot}_tone",
                [241, 238, 232, 255],
                12,
                "center",
                "middle",
                visible=False,
            )
        )
        dw.append(
            panel(
                f"dialogue_choice_{slot}_standing",
                "bottom_left",
                (standing_x, chip_y),
                (standing_w, 22),
                [72, 62, 48, 255],
                visible=False,
            )
        )
        dw.append(
            text(
                f"dialogue_choice_{slot}_standing_text",
                "bottom_left",
                (standing_x, chip_y),
                (standing_w, 22),
                f"dialogue.choice_{slot}_standing",
                [241, 238, 232, 255],
                12,
                "center",
                "middle",
                visible=False,
            )
        )

    print(
        "dialogue pads left",
        key_inset + key + 12,
        "right",
        edge + standing_w + chip_gap + tone_w,
        "prompt_y",
        prompt_y,
        "port_y",
        port_y,
    )
    return {
        "schemaVersion": 1,
        "id": "dialogue",
        "designResolution": [int(DW), int(DH)],
        "scaleMode": "letterbox",
        "widgets": dw,
    }


def main() -> None:
    player_path = UI / "player.uicanvas.json"
    dialogue_path = UI / "dialogue.uicanvas.json"
    player_path.write_text(json.dumps(build_player(), indent=4) + "\n", encoding="utf-8")
    dialogue_path.write_text(json.dumps(build_dialogue(), indent=4) + "\n", encoding="utf-8")
    print("wrote", player_path)
    print("wrote", dialogue_path)


if __name__ == "__main__":
    main()
