<p align="center">
  <img src="data/images/vtuber-effects-header.png" alt="VTuber Effects" width="420">
</p>

<p align="center">
  <strong>Dynamic environment lighting and finishing effects for transparent VTuber captures in OBS Studio.</strong>
</p>

<p align="center">
  <img alt="Version" src="https://img.shields.io/badge/version-1.0.0--beta2-8b5cf6">
  <img alt="OBS Studio" src="https://img.shields.io/badge/OBS-29.0.2%2B-302e31?logo=obsstudio">
  <img alt="Platforms" src="https://img.shields.io/badge/platform-Windows%20%7C%20macOS%20%7C%20Linux-38bdf8">
</p>

---

## What it does

VTuber Effects is a single OBS effect filter for transparent avatar sources such as Spout2 captures. It samples a game, video, scene, or source group and uses its broad colors and brightness to illuminate the avatar dynamically—without projecting recognizable background details onto it.

It combines the work of several filter chains into one configurable filter:

- Ambient environment lighting
- Directional rim lighting
- Protected emissive colors and bloom
- Automatic exposure compensation
- Drop shadow and outer glow
- Importable and exportable presets
- Diagnostic views for every processing stage

> [!IMPORTANT]
> This is beta software. Test your settings and keep a backup of your OBS scene collection before using it in a live production.

## Requirements

| Requirement | Minimum |
| --- | --- |
| OBS Studio | 29.0.2 |
| Avatar source | A video source with transparency |
| Environment source | A game, video, scene, or source group |

The avatar and environment work best when they share the same full-frame dimensions and aspect ratio.

## Installation

Download the appropriate package from the repository's **Releases** page. Close OBS before installing or replacing the plugin.

### Windows

Run the Windows installer when one is provided. For a manual ZIP installation, copy its contents into:

```text
C:\Program Files\obs-studio
```

The resulting files should include:

```text
obs-plugins\64bit\vtuber-effects.dll
data\obs-plugins\vtuber-effects\effects\
data\obs-plugins\vtuber-effects\images\
data\obs-plugins\vtuber-effects\locale\
```

### macOS

Open the macOS package and follow its installation prompts. If macOS blocks an unsigned beta package, open **System Settings → Privacy & Security** and approve it after confirming that it came from this repository.

### Linux

Install the provided Debian package on a compatible Debian or Ubuntu system, or extract the archive and copy the plugin library and data into the corresponding OBS plugin directories.

Restart OBS after installation.

## Quick start

1. Add the transparent avatar capture to an OBS scene.
2. Open the avatar source's **Filters** window.
3. Add **VTuber Effects** under **Effect Filters**.
4. Choose an **Environment source or group**.
5. Adjust Ambient Lighting and Rim Lighting to suit the scene.
6. Return **Diagnostic view** to **Final output** before streaming or recording.

The selected environment must not contain the filtered avatar itself. Doing so creates a rendering loop.

## Environment sources

### Environment source or group

The primary source sampled by Ambient Lighting and, by default, Rim Lighting. This can be a game capture, video, scene, or source group.

### Rim environment source

Optionally selects a separate environment for Rim Lighting. Leave it set to the primary environment option to reuse the main source.

## Diagnostic views

| View | Purpose |
| --- | --- |
| Final output | Completed filter result |
| Blurred environment | Smooth color texture sampled for lighting |
| Ambient pass | Ambient lighting by itself |
| Rim pass | Rim lighting by itself |
| Protected color mask | Pixels matched by protected colors |
| Original capture | Unmodified avatar input |
| Drop shadow pass | Shadow with transparent surroundings |
| Outer glow pass | Outer glow with transparent surroundings |
| Emissive bloom pass | Bloom generated from protected colors |

## Ambient lighting

Ambient Lighting covers the visible avatar with a heavily blurred version of the environment. The avatar inherits broad scene colors and brightness without showing recognizable background details.

| Setting | Description |
| --- | --- |
| Base brightness | Minimum brightness retained before environment color is applied |
| Environment color amount | Strength of the environment color on the avatar |
| Environment blur radius | How broadly the environment is averaged; higher values produce smoother lighting |

## Automatic exposure compensation

Processes the sampled environment before it is used as lighting. It can restrain unusually bright scenes, lift dark scenes, and tune color intensity.

| Setting | Description |
| --- | --- |
| Target environment brightness | Brightness that automatic exposure attempts to maintain |
| Exposure compensation strength | How strongly the calculated correction is applied |
| Minimum exposure multiplier | Maximum permitted darkening of bright environments |
| Maximum exposure multiplier | Maximum permitted brightening of dark environments |
| Affect rim lighting | Applies or bypasses exposure correction for the rim environment |
| Environment saturation | Overall saturation of sampled lighting colors |
| Environment vibrance | Primarily strengthens colors that are currently muted |
| Environment contrast | Contrast applied before colors are used as lighting |
| Maximum color intensity | Caps the strongest channel while preserving hue |

## Rim lighting

Rim Lighting creates a soft directional band inside the avatar's silhouette and colors it using the selected rim environment.

### Positioning modes

| Mode | Behavior |
| --- | --- |
| **Local silhouette (tracking-free)** | Builds the rim from neighboring alpha pixels. It automatically follows movement and scaling performed inside VTuber software and requires no pivot tracking. |
| **Scaled duplicate** | Transforms the complete alpha mask around an automatic or manual pivot. This preserves the original duplicate-and-mask style and supports Rim Mask Scale. |

Local Silhouette is recommended when animated overlays or chat emotes share the avatar capture. Those overlays may receive their own rim, but they cannot reposition the avatar's rim.

### Rim controls

| Setting | Description |
| --- | --- |
| Rim brightness | Overall rim intensity |
| Rim environment color amount | Strength of environment color in the rim |
| Rim minimum brightness | Minimum Masked Duplicate rim visible over dark environments |
| Rim darkness cutoff | Treats sufficiently dark environment samples as unlit |
| Rim blend mode | Additive, Screen, Normal Mix, or Masked Duplicate compositing |
| Rim width | Distance the rim extends inward from the alpha edge |
| Rim softness | Transition between the rim and the unlit avatar |
| Local silhouette expansion | Expands or contracts each local alpha edge in pixels without requiring a pivot |
| Rim mask scale | Scale of the complete mask in Scaled Duplicate mode |
| Follow avatar automatically | Recalculates the Scaled Duplicate pivot from the alpha silhouette |
| Lock rim pivot against overlays | Uses the manual pivot so animated overlays cannot move it |
| Rim scale pivot X/Y | Manual center used by Scaled Duplicate mode |
| Horizontal/vertical offset | Direction and displacement of the rim mask |

## Protected light colors

Protected Light Colors restore selected colors from the original avatar after lighting is applied. This keeps emissive eyes, lamps, displays, and indicators clear and unchanged. Up to ten independent colors can be configured.

| Setting | Description |
| --- | --- |
| Color | Source color to preserve |
| Color tolerance | Maximum difference from the selected color |
| Match softness | Feathering between matched and unmatched pixels |
| Minimum saturation | Rejects neutral or weakly colored pixels |
| Minimum brightness | Rejects dark pixels with a similar hue |
| Protection strength | Amount of the original pixel restored |

The first protected color defaults to red (`#FF0000`).

## Emissive bloom

Emissive Bloom creates a soft colored halo from pixels matched by Protected Light Colors. Bloom uses the original matched color, so red eyes produce red bloom and green indicators produce green bloom.

| Setting | Description |
| --- | --- |
| Emissive bloom brightness | Intensity of the emitted halo |
| Emissive bloom radius | Distance the bloom spreads from matched pixels |

## Drop shadow

Drop Shadow places a blurred, colored, and offset copy of the avatar silhouette behind the avatar. Controls include color, opacity, blur radius, and independent horizontal and vertical offsets.

## Outer glow

Outer Glow creates a smooth fixed-color halo outside the avatar. It is independent of environment lighting and Drop Shadow. Available blend modes are Additive, Screen, and Normal.

## Presets

The controls at the bottom of the filter can export the current effect settings to a JSON file or import a previously shared preset.

Presets include all effect controls but intentionally exclude environment source selections and Diagnostic View because those values depend on each user's OBS scene and source names.

## Update notifications

VTuber Effects can check its public GitHub releases for newer versions after OBS starts. The check runs at most once every 24 hours and does not send personal information, OBS settings, or usage data.

When an update is available, the notification can open its release page, skip that specific version, postpone the reminder, or disable automatic checks. The **Check for updates** button beneath the plugin version performs a manual check even when automatic checks are disabled or a release was previously skipped.

Beta installations are notified about newer beta, release-candidate, and stable versions. Stable installations ignore prereleases.

## Filter order

OBS processes filters from top to bottom:

- Filters **above** VTuber Effects modify the avatar before lighting is calculated.
- Filters **below** VTuber Effects modify the completed lighting, shadow, bloom, and glow.

## Performance tips

Large blur and bloom radii require more GPU work, especially at high resolutions. If performance is lower than expected:

- Reduce Environment Blur Radius.
- Reduce Rim Width.
- Reduce Emissive Bloom Radius.
- Reduce Glow Width.
- Reduce Shadow Blur Radius.
- Disable effects that are not being used.

## Troubleshooting

<details>
<summary><strong>VTuber Effects is not listed in OBS</strong></summary>

- Confirm that the plugin was installed for the same OBS installation you are opening.
- Restart OBS after installation.
- On Windows, confirm that `vtuber-effects.dll` is in `obs-plugins\64bit`.
- Confirm that the `effects`, `images`, and `locale` folders are under `data\obs-plugins\vtuber-effects`.
- Check **Help → Log Files → View Current Log** for plugin-loading errors.

</details>

<details>
<summary><strong>Environment lighting does not appear</strong></summary>

- Select an Environment source or group.
- Confirm that the selected source is visible and producing video.
- Enable Ambient Lighting or Rim Lighting.
- Do not select a scene or group that contains the filtered avatar.

</details>

<details>
<summary><strong>Protected colors match too much of the avatar</strong></summary>

- Lower that color entry's tolerance or softness.
- Raise its minimum saturation or minimum brightness.

</details>

<details>
<summary><strong>Chat emotes make Scaled Duplicate rim lighting move</strong></summary>

Use **Local silhouette (tracking-free)** positioning. Alternatively, enable **Lock rim pivot against overlays** and set the manual X/Y pivot to the avatar's usual location.

</details>

## Compatibility

This beta targets OBS Studio **29.0.2 and later** on Windows, macOS, and Linux. Builds are platform-specific; install the package made for your operating system.

## Version

Current beta: **1.0.0-beta2**

The beta number will remain at beta 2 while features and behavior are being evaluated.
