"""Hardware-neutral colour reduction, keeping source dimensions and pixel positions."""

from __future__ import annotations

import numpy as np
from PIL import Image


def rgb555_image(image):
    """Return the nearest console-grid representation without palette reduction."""
    values = np.asarray(image.convert("RGB"), dtype=np.uint8) >> 3
    return Image.fromarray((values << 3) | (values >> 2))


def banked_background(image, bank_count=8, colours_per_bank=15, iterations=6):
    """Fit shared palettes to 8x8 tile histograms, then assign one bank per tile.

    Palettes use weighted medoids: their entries are actual source colours on
    the RGB555 grid. No dithering, resizing or spatial filtering is performed.
    """
    if image.width % 8 or image.height % 8 or not 1 <= bank_count <= 16:
        raise ValueError("background dimensions must align to 8px and banks must be in 1..16")
    if not 1 <= colours_per_bank <= 15:
        raise ValueError("4bpp banks have at most 15 opaque colours")
    pixels = np.asarray(image.convert("RGB"), dtype=np.uint8) >> 3
    colours, inverse = np.unique(pixels.reshape(-1, 3), axis=0, return_inverse=True)
    colour_count = len(colours)
    identifiers = inverse.reshape(image.height, image.width)
    tile_ids = identifiers.reshape(image.height // 8, 8, image.width // 8, 8).transpose(0, 2, 1, 3).reshape(-1, 64)
    histograms = np.zeros((len(tile_ids), colour_count), dtype=np.int32)
    for index, tile in enumerate(tile_ids):
        histograms[index] = np.bincount(tile, minlength=colour_count)
    differences = colours.astype(np.float64)[:, None, :] - colours.astype(np.float64)[None, :, :]
    distances = (differences * differences).sum(axis=2)

    def fit(weights):
        active = np.flatnonzero(weights)
        if len(active) <= colours_per_bank:
            selected = active.tolist()
        else:
            selected = [int(np.argmax(weights))]
            for _ in range(colours_per_bank - 1):
                errors = distances[:, selected].min(axis=1) * weights
                selected.append(int(np.argmax(errors)))
            for _ in range(6):
                assignment = distances[:, selected].argmin(axis=1)
                updated = []
                for index, current in enumerate(selected):
                    members = active[assignment[active] == index]
                    if len(members):
                        costs = distances[np.ix_(members, members)] @ weights[members].astype(np.float64)
                        current = int(members[int(np.argmin(costs))])
                    updated.append(current)
                if updated == selected:
                    break
                selected = updated
        # give common colours consistent low slots where possible.
        selected = sorted(set(selected), key=lambda colour: (-int(total_weights[colour]), colour))
        return selected + [selected[0]] * (colours_per_bank - len(selected))

    total_weights = histograms.sum(axis=0)
    banks = [fit(total_weights)]
    for _ in range(bank_count - 1):
        errors = np.stack([distances[:, bank].min(axis=1) for bank in banks], axis=1)
        tile_errors = histograms @ errors
        worst = int(np.argmax(tile_errors.min(axis=1)))
        banks.append(fit(histograms[worst]))
    for _ in range(iterations):
        errors = np.stack([distances[:, bank].min(axis=1) for bank in banks], axis=1)
        assignments = (histograms @ errors).argmin(axis=1)
        updated = []
        for index in range(bank_count):
            members = histograms[assignments == index]
            updated.append(fit(members.sum(axis=0)) if len(members) else banks[index])
        if updated == banks:
            break
        banks = updated
    errors = np.stack([distances[:, bank].min(axis=1) for bank in banks], axis=1)
    assignments = (histograms @ errors).argmin(axis=1)
    palette = [0] * 768
    colour_lookup = []
    for bank_index, bank in enumerate(banks):
        colour_lookup.append(distances[:, bank].argmin(axis=1) + 1)
        for slot, colour in enumerate(bank, start=1):
            rgb = colours[colour]
            expanded = ((rgb << 3) | (rgb >> 2)).tolist()
            offset = (bank_index * 16 + slot) * 3
            palette[offset:offset + 3] = expanded
    result = np.zeros_like(identifiers, dtype=np.uint8)
    for index, bank in enumerate(assignments):
        left, top = (index % (image.width // 8)) * 8, (index // (image.width // 8)) * 8
        result[top:top + 8, left:left + 8] = colour_lookup[bank][identifiers[top:top + 8, left:left + 8]] + int(bank) * 16
    converted = Image.frombytes("P", image.size, result.tobytes())
    converted.putpalette(palette)
    quantized = np.asarray(converted.convert("RGB"), dtype=np.uint8) >> 3
    error = pixels.astype(np.float64) - quantized.astype(np.float64)
    metrics = {"palette_banks": bank_count, "used_palette_banks": len(set(assignments.tolist())),
               "source_rgb555_colours": colour_count,
               "output_rgb555_colours": len(np.unique(quantized.reshape(-1, 3), axis=0)),
               "rgb555_mean_squared_error": float((error * error).mean()),
               "exact_rgb555_pixels_fraction": float(np.all(error == 0, axis=2).mean()),
               "dithering": False, "resampling": False}
    return converted, assignments.tolist(), metrics


def palette_rgb555(image, bank_count):
    palette = image.getpalette()[:bank_count * 16 * 3]
    result = bytearray()
    for offset in range(0, len(palette), 3):
        red, green, blue = palette[offset:offset + 3]
        word = (red >> 3) | ((green >> 3) << 5) | ((blue >> 3) << 10)
        result += word.to_bytes(2, "little")
    return bytes(result)
