<div align="center">
  <img src="https://github.com/mangowm/mango/blob/main/assets/mango-transparency-256.png" alt="Mango Logo" width="120"/>

  <h1>Mango Wayland Compositor</h1>

  <p>A fast, feature-rich Wayland compositor built on <a href="https://codeberg.org/dwl/dwl">dwl</a></p>

<a href="https://github.com/mangowm/mango/stargazers"><img src="https://img.shields.io/github/stars/mangowm/mango?style=flat&color=orange" alt="Stars"/></a>
<a href="https://github.com/mangowm/mango/blob/main/LICENSE"><img src="https://img.shields.io/badge/license-GPL--3.0-blue?style=flat" alt="License"/></a>
<a href="https://repology.org/project/mangowm/versions"><img src="https://repology.org/badge/tiny-repos/mangowm.svg" alt="Packaged in"/></a>
<a href="https://discord.gg/CPjbDxesh5"><img src="https://img.shields.io/discord/1430889676264177687?style=flat&logo=discord&label=discord" alt="Discord"/></a>

</div>

---

## swango — what this fork adds

**swango** is a fork of [mango](https://github.com/mangowm/mango) (branch `wl-only`).
The name is sway + mango: it brings over what sway and Hyprland do better on HDR, and
fixes what was broken along the way. Everything below was measured on hardware, on two
different panels — a Samsung ATNA40CU05-0 OLED and an RTK ZEUSLAP TYPEC — under
wlroots 0.20 with the Vulkan renderer.

### Mastering display metadata

Upstream fills two of the six fields of `wlr_output_image_description`. The remaining
four stay zero, so the panel receives an HDR infoframe with 0.0 primaries and zero
luminance and has to guess how to tone-map. Four `monitorrule` keys fix that:

| key | meaning |
| :--- | :--- |
| `hdr_min_lum` | mastering minimum luminance, cd/m² |
| `hdr_max_lum` | mastering peak, also sent as `max_cll` |
| `hdr_max_avg_lum` | `max_fall` |
| `hdr_force` | enable HDR even when the EDID does not advertise BT.2020/PQ |

```ini
monitorrule=name:eDP-1,...,hdr:1,hdr_max_lum:616.884,hdr_max_avg_lum:400
```

Leave them unset and the fields stay zero — upstream behaviour, byte for byte.

`hdr_force` exists for panels that declare HDR only inside a **DisplayID 2.0**
extension, with the CTA-861 blocks nested in a `0x81` container. That is legal EDID
1.4, but libdisplay-info's CTA path never descends into it, so `hdr:1` is silently
dropped on hardware that drives PQ fine.

### Runtime HDR toggle

The equivalent of sway's `output <name> hdr on|off|toggle`. Upstream can only set HDR
from `monitorrule`, so changing it means a config reload that re-applies mode, scale,
position and transform on every output.

```sh
mmsg dispatch togglehdr              # toggle the focused monitor
mmsg dispatch togglehdr,off,eDP-1    # a named output
mmsg dispatch togglehdr,toggle,all   # every output at once
```

With `all`, toggle takes **one** decision for every output — if anything is on,
everything goes off — instead of flipping each monitor against its own state, which
would leave a multi-monitor desk half on and half off.

### Two fixes

**Stale regions after an HDR switch.** wlroots damages the whole output for a
geometry, transform, scale or rendered-gamma change, but not for an image description
change — even though that changes how every pixel must be encoded. Anything left
undamaged kept the luminance mapping it had when last drawn, so a static desktop was
left with flat rectangles at the wrong brightness. swango damages the output itself
before committing.

**Crash on display unplug.** `xdg_output_cleanup_output()` called
`wl_resource_destroy()` on a `zxdg_output_v1`, an object created by the *client*.
libwayland then sends `delete_id`, the client reuses the id while its own destroy
request is still in flight, and the next request hits an unknown object — a fatal
`invalid object` that kills the client. With a layer-shell bar running, that took down
the bar and the shell on every unplug. Fixed by making the resource inert, the idiom
wlroots uses everywhere. Proposed upstream as
[mangowm/mango#1258](https://github.com/mangowm/mango/pull/1258).

---

https://github.com/user-attachments/assets/bb83004a-0563-4b48-ad89-6461a9b78b1f

> See all layouts in action at [mangowm.github.io](https://mangowm.github.io/)

## Why Mango?

Mango starts where dwl ends. It keeps the lightweight, fast-build philosophy while adding the features that make a compositor actually usable day-to-day — without the bloat.

- **Lightweight & fast** — as lean as dwl, builds in seconds, no functionality compromised
- **Excellent xwayland support** — run X11 apps without friction
- **Tags, not workspaces** — each tag maintains its own independent window layout
- **Smooth animations** — window open/move/close, tag transitions, layer surfaces
- **Flexible layouts** — scroller, master-stack, monocle, dwindle, grid, and more
- **Rich window states** — swallow, minimize, maximize, global, overlay, fakefullscreen
- **Window effects** — blur, shadow, corner radius, opacity (via scenefx)
- **Excellent input method support** — text-input v2/v3
- **Sway-like scratchpad** — named scratchpad support included
- **Hycov-style overview** — see all windows at a glance
- **IPC** — send/receive messages from external programs
- **Hot-reload config** — no restart needed for keybinding changes
- **Zero flickering** — every frame is correct

## Vision

**Stability first.** After months of testing, Mango is solid enough for daily use. Breaking changes will be minimal.

**Practicality over novelty.** Features get added when they genuinely improve daily workflows — not for the sake of completeness.

**Focused scope.** Niche requests are evaluated by community interest. Significant upvotes move things forward.

## Installation

[![Packaging status](https://repology.org/badge/vertical-allrepos/mangowm.svg)](https://repology.org/project/mangowm/versions)

### Arch Linux

```bash
yay -S mangowm-git
```
#### use my config
- install dependencies
```
yay -S rofi foot xdg-desktop-portal-wlr swaybg waybar wl-clip-persist cliphist wl-clipboard wlsunset xfce-polkit swaync pamixer wlr-dpms sway-audio-idle-inhibit-git swayidle dimland-git brightnessctl swayosd wlr-randr grim slurp satty swaylock-effects-git wlogout sox
```
- clone config
```
git clone https://github.com/DreamMaoMao/mango-config.git ~/.config/mango
```

### Other distributions

See the [Installation Guide](https://mangowm.github.io/docs/installation) for Fedora, Gentoo, Guix, NixOS, openSUSE, PikaOS, AerynOS, and building from source.

## Documentation

- **[mangowm.github.io](https://mangowm.github.io/)** — website docs with configuration reference, keybindings, layouts, IPC, and more
- **[GitHub Wiki](https://github.com/mangowm/mango/wiki/)** — community-maintained wiki

## Community

Join us on **[Discord](https://discord.gg/CPjbDxesh5)**

## Acknowledgements

- [wlroots](https://gitlab.freedesktop.org/wlroots/wlroots) — Wayland protocol implementation
- [dwl](https://codeberg.org/dwl/dwl) — the foundation Mango builds on
- [scenefx](https://github.com/wlrfx/scenefx) — window effects library
- [owl](https://github.com/dqrk0jeste/owl) — animation groundwork
- [sway](https://github.com/swaywm/sway) — protocol reference

## Sponsor

If Mango makes your desktop better, consider supporting its development.

Thanks to everyone who has sponsored this project:

<table>
  <tr>
    <!-- add new sponsors here: copy the <td>...</td> block below -->
    <td align="center">
      <a href="https://github.com/dl09r">
        <img src="https://unavatar.io/github/dl09r" width="48" style="border-radius:50%"/><br/>
        <sub>dl09r</sub>
      </a>
    </td>
    <td align="center">
      <a href="https://github.com/tonybanters">
        <img src="https://unavatar.io/github/tonybanters" width="48" style="border-radius:50%"/><br/>
        <sub>tonybanters</sub>
      </a>
    </td>
    <td align="center">
      <a href="https://github.com/vinthara">
        <img src="https://unavatar.io/github/vinthara" width="48" style="border-radius:50%"/><br/>
        <sub>vinthara</sub>
      </a>
    </td>
  </tr>
</table>

Crypto donations accepted:

<table>
  <tr>
    <td valign="middle">
      <strong>Network:</strong> BEP20 (BSC)<br/>
      <strong>Address:</strong> <code>0xf9cda472f2556671d2504afc4c35340ec5615da1</code>
    </td>
    <td valign="middle">
      <img width="120" alt="sponsor QR" src="assets/crypto_sponserme_qrcode.png" />
    </td>
  </tr>
</table>
