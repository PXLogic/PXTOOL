/*
 * This file is part of the PXTOOL project.
 * PXTOOL is based on PulseView.
 *
 * Copyright (C) 2026 DreamSourceLab <support@dreamsourcelab.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "libsigrok-internal.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

SR_PRIV int sr_analog_init(struct sr_datafeed_analog *analog,
        struct sr_analog_encoding *encoding,
        struct sr_analog_meaning *meaning,
        struct sr_analog_spec *spec, int digits)
{
    if (!analog || !encoding || !meaning || !spec)
        return SR_ERR_ARG;

    memset(analog, 0, sizeof(*analog));
    memset(encoding, 0, sizeof(*encoding));
    memset(meaning, 0, sizeof(*meaning));
    memset(spec, 0, sizeof(*spec));

    analog->encoding = encoding;
    analog->meaning = meaning;
    analog->spec = spec;

    encoding->unitsize = sizeof(float);
    encoding->is_float = TRUE;
#ifdef WORDS_BIGENDIAN
    encoding->is_bigendian = TRUE;
#else
    encoding->is_bigendian = FALSE;
#endif
    encoding->digits = digits;
    encoding->is_digits_decimal = TRUE;
    encoding->scale.p = 1;
    encoding->scale.q = 1;
    encoding->offset.q = 1;
    spec->spec_digits = digits;

    return SR_OK;
}

static uint64_t read_unsigned_sample(const uint8_t *data, size_t unitsize,
        gboolean bigendian)
{
    uint64_t value = 0;
    size_t i;

    if (bigendian) {
        for (i = 0; i < unitsize; i++)
            value = (value << 8) | data[i];
    } else {
        for (i = 0; i < unitsize; i++)
            value |= (uint64_t)data[i] << (i * 8);
    }

    return value;
}

static int64_t read_signed_sample(const uint8_t *data, size_t unitsize,
        gboolean bigendian)
{
    uint64_t value = read_unsigned_sample(data, unitsize, bigendian);
    unsigned int bits = unitsize * 8;

    if (bits < 64 && value & ((uint64_t)1 << (bits - 1)))
        value |= ~((UINT64_C(1) << bits) - 1);

    return (int64_t)value;
}

SR_API int sr_analog_to_float(const struct sr_datafeed_analog *analog,
        float *outbuf)
{
    const struct sr_analog_encoding *encoding;
    const uint8_t *input;
    size_t count;
    size_t i;
    double scale;
    double offset;

    if (!analog || !analog->data || !analog->meaning || !analog->encoding ||
        !outbuf)
        return SR_ERR_ARG;

    encoding = analog->encoding;
    if (!encoding->unitsize || encoding->unitsize > sizeof(uint64_t) ||
        !encoding->scale.q || !encoding->offset.q)
        return SR_ERR_ARG;

    count = (size_t)analog->num_samples * g_slist_length(analog->meaning->channels);
    scale = (double)encoding->scale.p / encoding->scale.q;
    offset = (double)encoding->offset.p / encoding->offset.q;
    input = analog->data;

    for (i = 0; i < count; i++) {
        double value;

        if (encoding->is_float && encoding->unitsize == sizeof(float)) {
            uint32_t raw = (uint32_t)read_unsigned_sample(input,
                encoding->unitsize, encoding->is_bigendian);
            float sample;
            memcpy(&sample, &raw, sizeof(sample));
            value = sample;
        } else if (encoding->is_float && encoding->unitsize == sizeof(double)) {
            uint64_t raw = read_unsigned_sample(input, encoding->unitsize,
                encoding->is_bigendian);
            double sample;
            memcpy(&sample, &raw, sizeof(sample));
            value = sample;
        } else if (encoding->is_float) {
            return SR_ERR;
        } else if (encoding->is_signed) {
            value = read_signed_sample(input, encoding->unitsize,
                encoding->is_bigendian);
        } else {
            value = read_unsigned_sample(input, encoding->unitsize,
                encoding->is_bigendian);
        }

        outbuf[i] = (float)(value * scale + offset);
        input += encoding->unitsize;
    }

    return SR_OK;
}

SR_API const char *sr_analog_si_prefix(float *value, int *digits)
{
    static const char *prefixes[] = {
        "f", "p", "n", "\xc2\xb5", "m", "", "k", "M", "G", "T"
    };
    const int zero_index = 5;
    int prefix;
    float logarithm;

    if (!value || !digits || isnan(*value) || *value == 0.0f)
        return prefixes[zero_index];

    logarithm = log10f(fabsf(*value));
    prefix = (int)(logarithm / 3) - (logarithm < 1);
    if (prefix < -zero_index)
        prefix = -zero_index;
    if (3 * prefix < -*digits)
        prefix = (-*digits + 2 * (*digits < 0)) / 3;
    if (prefix > 4)
        prefix = 4;

    *value *= powf(10.0f, -3.0f * prefix);
    *digits += 3 * prefix;
    return prefixes[prefix + zero_index];
}

SR_API gboolean sr_analog_si_prefix_friendly(enum sr_unit unit)
{
    switch (unit) {
    case SR_UNIT_VOLT:
    case SR_UNIT_AMPERE:
    case SR_UNIT_OHM:
    case SR_UNIT_FARAD:
    case SR_UNIT_KELVIN:
    case SR_UNIT_HERTZ:
    case SR_UNIT_SECOND:
    case SR_UNIT_SIEMENS:
        return TRUE;
    default:
        return FALSE;
    }
}

SR_API int sr_analog_unit_to_string(const struct sr_datafeed_analog *analog,
        char **result)
{
    const char *unit = "";
    GString *text;

    if (!analog || !analog->meaning || !result)
        return SR_ERR_ARG;

    switch (analog->meaning->unit) {
    case SR_UNIT_VOLT: unit = "V"; break;
    case SR_UNIT_AMPERE: unit = "A"; break;
    case SR_UNIT_OHM: unit = "\xe2\x84\xa6"; break;
    case SR_UNIT_FARAD: unit = "F"; break;
    case SR_UNIT_KELVIN: unit = "K"; break;
    case SR_UNIT_CELSIUS: unit = "\xc2\xb0" "C"; break;
    case SR_UNIT_FAHRENHEIT: unit = "\xc2\xb0" "F"; break;
    case SR_UNIT_HERTZ: unit = "Hz"; break;
    case SR_UNIT_PERCENTAGE: unit = "%"; break;
    case SR_UNIT_SECOND: unit = "s"; break;
    case SR_UNIT_SIEMENS: unit = "S"; break;
    case SR_UNIT_DECIBEL_MW: unit = "dBm"; break;
    case SR_UNIT_DECIBEL_VOLT: unit = "dBV"; break;
    default: break;
    }

    text = g_string_new(unit);
    if (analog->meaning->mqflags & SR_MQFLAG_AC)
        g_string_append(text, " AC");
    if (analog->meaning->mqflags & SR_MQFLAG_DC)
        g_string_append(text, " DC");
    if (analog->meaning->mqflags & SR_MQFLAG_RMS)
        g_string_append(text, " RMS");
    if (analog->meaning->mqflags & SR_MQFLAG_HOLD)
        g_string_append(text, " HOLD");
    if (analog->meaning->mqflags & SR_MQFLAG_MAX)
        g_string_append(text, " MAX");
    if (analog->meaning->mqflags & SR_MQFLAG_MIN)
        g_string_append(text, " MIN");

    *result = g_string_free(text, FALSE);
    return SR_OK;
}

SR_API void sr_rational_set(struct sr_rational *rational, int64_t p,
        uint64_t q)
{
    if (!rational)
        return;

    rational->p = p;
    rational->q = q;
}
