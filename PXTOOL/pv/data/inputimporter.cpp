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
 * along with this program.  If not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301 USA
 */

#include "inputimporter.h"

#include <QFile>
#include <QFileInfo>
#include <QStringList>

#include "../log.h"
#include "../sigsession.h"

extern "C" {
#include "libsigrok-internal.h"
}

namespace pv {
namespace data {

namespace {

constexpr qint64 kChunkSize = 512 * 1024;

struct ImportPlan {
    int workMode = LOGIC;
    uint64_t sampleRate = 1;
    uint64_t sampleLimit = 1;
    bool preserveHeaderSampleLimit = false;
};

sr_dev_inst *cloneImportedDevice(const sr_dev_inst *source)
{
    if (!source)
        return nullptr;

    sr_dev_inst *clone = static_cast<sr_dev_inst *>(g_malloc0(sizeof(sr_dev_inst)));
    clone->mode = source->mode;
    clone->status = source->status;
    clone->dev_type = source->dev_type;
    clone->source_kind = source->source_kind;
    clone->index = source->index;
    clone->handle = reinterpret_cast<ds_device_handle>(clone);

    if (source->name)
        clone->name = g_strdup(source->name);
    if (source->path)
        clone->path = g_strdup(source->path);
    if (source->vendor)
        clone->vendor = g_strdup(source->vendor);
    if (source->version)
        clone->version = g_strdup(source->version);

    QHash<const sr_channel *, sr_channel *> channelMap;
    for (const GSList *l = source->channels; l; l = l->next) {
        const sr_channel *srcChannel = static_cast<const sr_channel *>(l->data);
        sr_channel *dstChannel = sr_channel_new(clone, srcChannel->index,
            srcChannel->type, srcChannel->enabled, srcChannel->name);
        if (!dstChannel)
            continue;

        dstChannel->bits = srcChannel->bits;
        dstChannel->vdiv = srcChannel->vdiv;
        dstChannel->vfactor = srcChannel->vfactor;
        dstChannel->offset = srcChannel->offset;
        dstChannel->zero_offset = srcChannel->zero_offset;
        dstChannel->hw_offset = srcChannel->hw_offset;
        dstChannel->vpos_trans = srcChannel->vpos_trans;
        dstChannel->coupling = srcChannel->coupling;
        dstChannel->trig_value = srcChannel->trig_value;
        dstChannel->comb_diff_top = srcChannel->comb_diff_top;
        dstChannel->comb_diff_bom = srcChannel->comb_diff_bom;
        dstChannel->comb_comp = srcChannel->comb_comp;
        dstChannel->digi_fgain = srcChannel->digi_fgain;
        dstChannel->cali_fgain0 = srcChannel->cali_fgain0;
        dstChannel->cali_fgain1 = srcChannel->cali_fgain1;
        dstChannel->cali_fgain2 = srcChannel->cali_fgain2;
        dstChannel->cali_fgain3 = srcChannel->cali_fgain3;
        dstChannel->cali_comb_fgain0 = srcChannel->cali_comb_fgain0;
        dstChannel->cali_comb_fgain1 = srcChannel->cali_comb_fgain1;
        dstChannel->cali_comb_fgain2 = srcChannel->cali_comb_fgain2;
        dstChannel->cali_comb_fgain3 = srcChannel->cali_comb_fgain3;
        dstChannel->map_default = srcChannel->map_default;
        dstChannel->map_unit = srcChannel->map_unit ? g_strdup(srcChannel->map_unit) : nullptr;
        dstChannel->map_min = srcChannel->map_min;
        dstChannel->map_max = srcChannel->map_max;
        if (srcChannel->trigger)
            dstChannel->trigger = g_strdup(srcChannel->trigger);

        channelMap.insert(srcChannel, dstChannel);
    }

    for (const GSList *l = source->channel_groups; l; l = l->next) {
        const sr_channel_group *srcGroup = static_cast<const sr_channel_group *>(l->data);
        sr_channel_group *dstGroup = sr_channel_group_new(clone, srcGroup->name, nullptr);
        if (!dstGroup)
            continue;

        for (const GSList *groupChannel = srcGroup->channels; groupChannel; groupChannel = groupChannel->next) {
            const sr_channel *srcChannel = static_cast<const sr_channel *>(groupChannel->data);
            sr_channel *dstChannel = channelMap.value(srcChannel, nullptr);
            if (dstChannel)
                dstGroup->channels = g_slist_append(dstGroup->channels, dstChannel);
        }
    }

    return clone;
}

void discard_datafeed_callback(const struct sr_dev_inst *, const struct sr_datafeed_packet *)
{
}

unsigned int channelCount(const sr_dev_inst *sdi)
{
    return sdi ? static_cast<unsigned int>(g_slist_length(sdi->channels)) : 0U;
}

unsigned int enabledChannelCount(const sr_dev_inst *sdi)
{
    unsigned int count = 0;
    if (!sdi)
        return count;

    for (const GSList *l = sdi->channels; l; l = l->next) {
        const sr_channel *channel = static_cast<const sr_channel *>(l->data);
        if (channel && channel->enabled)
            count++;
    }
    return count;
}

QVariant explicitOptionValue(const IoOptions &options, const QString &id)
{
    try {
        return options.value(id);
    } catch (...) {
        return QVariant();
    }
}

QVariant defaultOptionValue(const sr_input_module *module, const QString &id)
{
    if (!module || !module->options)
        return QVariant();

    const sr_option *defs = module->options();
    if (!defs)
        return QVariant();

    for (int i = 0; defs[i].id; ++i) {
        if (id != QString::fromUtf8(defs[i].id) || !defs[i].def)
            continue;
        if (g_variant_is_of_type(defs[i].def, G_VARIANT_TYPE_INT32))
            return QVariant(g_variant_get_int32(defs[i].def));
        if (g_variant_is_of_type(defs[i].def, G_VARIANT_TYPE_UINT32))
            return QVariant::fromValue(g_variant_get_uint32(defs[i].def));
        if (g_variant_is_of_type(defs[i].def, G_VARIANT_TYPE_UINT64))
            return QVariant::fromValue(g_variant_get_uint64(defs[i].def));
        if (g_variant_is_of_type(defs[i].def, G_VARIANT_TYPE_BOOLEAN))
            return QVariant(g_variant_get_boolean(defs[i].def) != FALSE);
    }

    return QVariant();
}

QVariant optionValue(const IoOptions &options,
                     const sr_input_module *module,
                     const QString &primaryId,
                     const QString &aliasId = QString())
{
    QVariant value = explicitOptionValue(options, primaryId);
    if (value.isValid())
        return value;

    if (!aliasId.isEmpty()) {
        value = explicitOptionValue(options, aliasId);
        if (value.isValid())
            return value;
    }

    value = defaultOptionValue(module, primaryId);
    if (value.isValid())
        return value;

    if (!aliasId.isEmpty())
        return defaultOptionValue(module, aliasId);

    return QVariant();
}

uint64_t clampSampleLimit(uint64_t sampleLimit)
{
    return sampleLimit == 0 ? 1 : sampleLimit;
}

int inferWorkMode(const sr_dev_inst *sdi, int fallback)
{
    bool hasLogic = false;
    bool hasAnalog = false;

    if (sdi) {
        for (const GSList *l = sdi->channels; l; l = l->next) {
            const sr_channel *channel = static_cast<const sr_channel *>(l->data);
            if (channel->type == SR_CHANNEL_LOGIC)
                hasLogic = true;
            else if (channel->type == SR_CHANNEL_ANALOG)
                hasAnalog = true;
        }
    }

    if (hasLogic)
        return LOGIC;
    if (hasAnalog)
        return ANALOG;
    return fallback;
}

bool parseTimescaleSamplerate(const QString &text, uint64_t &sampleRate)
{
    QByteArray bytes = text.trimmed().toUtf8();
    uint64_t p = 0;
    uint64_t q = 0;
    if (sr_parse_period(bytes.constData(), &p, &q) != SR_OK || p == 0)
        return false;
    sampleRate = q / p;
    return sampleRate != 0;
}

ImportPlan estimateBinaryImport(const QString &fileName,
                                const sr_input_module *module,
                                const IoOptions &options)
{
    ImportPlan plan;
    plan.workMode = LOGIC;

    const QVariant numChannelsOpt = optionValue(options, module,
        QStringLiteral("numchannels"), QStringLiteral("numprobes"));
    const uint64_t numChannels = numChannelsOpt.isValid()
        ? static_cast<uint64_t>(numChannelsOpt.toUInt())
        : 8ULL;
    const uint64_t unitSize = qMax<uint64_t>(1, (numChannels + 7) / 8);
    const uint64_t fileSize = static_cast<uint64_t>(QFileInfo(fileName).size());

    const QVariant sampleRateOpt = optionValue(options, module, QStringLiteral("samplerate"));
    if (sampleRateOpt.isValid() && sampleRateOpt.toULongLong() != 0)
        plan.sampleRate = sampleRateOpt.toULongLong();

    plan.sampleLimit = clampSampleLimit(fileSize / unitSize);
    return plan;
}

ImportPlan estimateChronovuImport(const QString &fileName,
                                  const sr_input_module *module,
                                  const IoOptions &options)
{
    constexpr uint64_t kChronovuDataSize = 8ULL * 1024 * 1024;
    ImportPlan plan;
    plan.workMode = LOGIC;

    const QVariant numChannelsOpt = optionValue(options, module,
        QStringLiteral("numchannels"), QStringLiteral("numprobes"));
    const uint64_t numChannels = numChannelsOpt.isValid()
        ? static_cast<uint64_t>(numChannelsOpt.toUInt())
        : 8ULL;
    const uint64_t unitSize = qMax<uint64_t>(1, (numChannels + 7) / 8);
    const uint64_t fileSize = static_cast<uint64_t>(QFileInfo(fileName).size());
    const uint64_t dataSize = qMin(fileSize, kChronovuDataSize);
    const QVariant samplerateOpt = optionValue(options, module,
        QStringLiteral("samplerate"));

    plan.sampleRate = samplerateOpt.isValid()
        ? samplerateOpt.toULongLong()
        : 1ULL;
    plan.sampleLimit = clampSampleLimit(dataSize / unitSize);
    return plan;
}

ImportPlan estimateWavImport(const QString &fileName)
{
    ImportPlan plan;
    plan.workMode = ANALOG;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly))
        return plan;

    const QByteArray header = file.read(1024);
    if (header.size() < 44)
        return plan;

    auto rl16 = [&](int offset) -> uint16_t {
        return static_cast<uint16_t>(
            static_cast<unsigned char>(header[offset]) |
            (static_cast<unsigned char>(header[offset + 1]) << 8));
    };
    auto rl32 = [&](int offset) -> uint32_t {
        return static_cast<uint32_t>(
            static_cast<unsigned char>(header[offset]) |
            (static_cast<unsigned char>(header[offset + 1]) << 8) |
            (static_cast<unsigned char>(header[offset + 2]) << 16) |
            (static_cast<unsigned char>(header[offset + 3]) << 24));
    };

    if (header.mid(0, 4) != "RIFF" || header.mid(8, 4) != "WAVE")
        return plan;

    plan.sampleRate = qMax<uint64_t>(1, rl32(24));
    const uint32_t blockAlign = rl16(32);

    int offset = 12;
    while (offset + 8 <= header.size()) {
        const QByteArray chunkId = header.mid(offset, 4);
        const uint32_t chunkSize = rl32(offset + 4);
        if (chunkId == "data") {
            if (blockAlign != 0)
                plan.sampleLimit = clampSampleLimit(chunkSize / blockAlign);
            return plan;
        }
        offset += 8 + static_cast<int>(chunkSize);
    }

    return plan;
}

ImportPlan estimateVcdImport(const QString &fileName,
                             const sr_input_module *module,
                             const IoOptions &options)
{
    ImportPlan plan;
    plan.workMode = LOGIC;

    const QVariant sampleRateOverride = optionValue(options, module,
        QStringLiteral("samplerate_overwrite"));
    const QVariant downsampleOpt = optionValue(options, module, QStringLiteral("downsample"));
    const QVariant compressOpt = optionValue(options, module, QStringLiteral("compress"));
    const QVariant skipOpt = optionValue(options, module, QStringLiteral("skip"));

    const uint64_t downsample = qMax<uint64_t>(1, downsampleOpt.toULongLong());
    uint64_t compress = compressOpt.toULongLong();
    compress /= downsample;

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
        return plan;

    uint64_t firstTimestamp = 0;
    uint64_t prevTimestamp = 0;
    bool sawTimestamp = false;
    bool dataAfterTimestamp = false;
    bool hasLogic = false;
    bool hasAnalog = false;

    while (!file.atEnd()) {
        const QString rawLine = QString::fromUtf8(file.readLine()).trimmed();
        if (rawLine.isEmpty())
            continue;

        if (rawLine.startsWith(QStringLiteral("$timescale"))) {
            QString contents = rawLine;
            contents.remove(QStringLiteral("$timescale"));
            contents.remove(QStringLiteral("$end"));
            uint64_t parsedRate = 0;
            if (parseTimescaleSamplerate(contents, parsedRate))
                plan.sampleRate = parsedRate;
            continue;
        }

        if (rawLine.startsWith(QStringLiteral("$var"))) {
            const QStringList parts = rawLine.split(QChar(' '), Qt::SkipEmptyParts);
            if (parts.size() >= 5) {
                const QString type = parts[1];
                const int width = parts[2].toInt();
                if ((type == QLatin1String("wire") ||
                     type == QLatin1String("reg") ||
                     type == QLatin1String("logic")) && width == 1) {
                    hasLogic = true;
                } else if (type == QLatin1String("integer") ||
                           type == QLatin1String("real")) {
                    hasAnalog = true;
                }
            }
            continue;
        }

        if (rawLine.startsWith(QLatin1Char('#'))) {
            uint64_t timestamp = 0;
            if (!parseVcdTimestampLine(rawLine, timestamp))
                continue;
            timestamp /= downsample;
            if (!sawTimestamp) {
                firstTimestamp = timestamp;
                prevTimestamp = timestamp;
                sawTimestamp = true;
            } else {
                prevTimestamp = qMax(prevTimestamp, timestamp);
            }
            dataAfterTimestamp = false;
            continue;
        }

        if (sawTimestamp)
            dataAfterTimestamp = true;
    }

    if (sampleRateOverride.isValid() && sampleRateOverride.toULongLong() != 0)
        plan.sampleRate = sampleRateOverride.toULongLong();

    if (hasAnalog && !hasLogic)
        plan.workMode = ANALOG;

    if (!sawTimestamp) {
        plan.sampleLimit = 1;
        return plan;
    }

    uint64_t startTimestamp = firstTimestamp;
    const uint64_t skipRaw = skipOpt.toULongLong();
    if (skipOpt.isValid() && skipRaw != ~UINT64_C(0))
        startTimestamp = skipRaw / downsample;

    file.seek(0);
    uint64_t totalSamples = 0;
    uint64_t effectivePrev = startTimestamp;
    bool firstSeen = false;
    dataAfterTimestamp = false;

    while (!file.atEnd()) {
        const QString rawLine = QString::fromUtf8(file.readLine()).trimmed();
        if (rawLine.isEmpty())
            continue;
        if (rawLine.startsWith(QLatin1Char('#'))) {
            uint64_t timestamp = 0;
            if (!parseVcdTimestampLine(rawLine, timestamp))
                continue;
            timestamp /= downsample;
            if (!firstSeen) {
                firstSeen = true;
                if (skipOpt.isValid() && skipRaw == ~UINT64_C(0)) {
                    effectivePrev = timestamp;
                    dataAfterTimestamp = false;
                    continue;
                }
            }
            if (timestamp < effectivePrev) {
                dataAfterTimestamp = false;
                continue;
            }
            uint64_t delta = timestamp - effectivePrev;
            if (compress > 0 && delta > compress)
                delta = compress;
            totalSamples += delta;
            effectivePrev = timestamp;
            dataAfterTimestamp = false;
            continue;
        }
        if (firstSeen)
            dataAfterTimestamp = true;
    }

    if (dataAfterTimestamp)
        totalSamples += 1;

    plan.sampleLimit = clampSampleLimit(totalSamples);
    return plan;
}

ImportPlan makeImportPlan(const QString &formatId,
                          const QString &fileName,
                          const sr_input_module *module,
                          const IoOptions &options)
{
    if (formatId == QLatin1String("csv")) {
        CsvImportPlan csv_plan;
        if (estimateDsViewCsvImportPlan(fileName, csv_plan)) {
            ImportPlan plan;
            plan.workMode = LOGIC;
            plan.sampleRate = csv_plan.sampleRate;
            plan.sampleLimit = csv_plan.sampleLimit;
            plan.preserveHeaderSampleLimit = true;
            return plan;
        }
    }
    if (formatId == QLatin1String("binary"))
        return estimateBinaryImport(fileName, module, options);
    if (formatId == QLatin1String("chronovu-la8"))
        return estimateChronovuImport(fileName, module, options);
    if (formatId == QLatin1String("wav"))
        return estimateWavImport(fileName);
    if (formatId == QLatin1String("vcd"))
        return estimateVcdImport(fileName, module, options);

    ImportPlan plan;
    return plan;
}

} // namespace

ImportResult InputImporter::importFile(SigSession &session,
                                       const QString &formatId,
                                       const QString &fileName,
                                       const IoOptions &options)
{
    ImportResult result;

    if (formatId.isEmpty()) {
        result.error = QStringLiteral("Import format is empty.");
        return result;
    }
    if (fileName.isEmpty()) {
        result.error = QStringLiteral("Import file name is empty.");
        return result;
    }

    const sr_input_module *module = sr_input_find(formatId.toUtf8().constData());
    if (!module) {
        result.error = QStringLiteral("Unsupported import format: %1").arg(formatId);
        return result;
    }

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly)) {
        result.error = QStringLiteral("Failed to open %1 for import.").arg(fileName);
        return result;
    }

    const ImportPlan plan = makeImportPlan(formatId, fileName, module, options);
    dsv_info("InputImporter: plan format=%s file=%s work_mode=%d samplerate=%llu samplelimit=%llu",
             formatId.toUtf8().constData(),
             fileName.toUtf8().constData(),
             plan.workMode,
             (unsigned long long)plan.sampleRate,
             (unsigned long long)plan.sampleLimit);
    if (formatId == QLatin1String("csv")) {
        result.sampleRate = plan.sampleRate;
        result.sampleLimit = plan.sampleLimit;
    }
    const QString displayName = QFileInfo(fileName).completeBaseName();
    auto make_input = [&]() -> sr_input * {
        GHashTable *option_table = nullptr;
        if (!options.empty())
            option_table = options.toGHashTable();

        sr_input *input = sr_input_new(module, option_table);
        if (option_table)
            g_hash_table_destroy(option_table);
        return input;
    };
    auto send_file = [&](sr_input *input) -> bool {
        file.seek(0);
        while (!file.atEnd()) {
            const QByteArray chunk = file.read(kChunkSize);
            if (chunk.isEmpty() && file.error() != QFileDevice::NoError) {
                result.error = QStringLiteral("Failed to read %1 during import.").arg(fileName);
                return false;
            }

            GString *buffer = g_string_new_len(chunk.constData(), chunk.size());
            const int rc = sr_input_send(input, buffer);
            g_string_free(buffer, TRUE);
            if (rc != SR_OK) {
                result.error = QStringLiteral("Failed while parsing %1.").arg(fileName);
                return false;
            }
        }

        // Some streaming inputs use the first receive call only to publish
        // their device description. Give them one empty receive to process
        // data supplied in that same call when the file fits one chunk.
        if (input->sdi_ready) {
            GString *flush = g_string_new("");
            const int rc = sr_input_send(input, flush);
            g_string_free(flush, TRUE);
            if (rc != SR_OK) {
                result.error = QStringLiteral("Failed while flushing %1.").arg(fileName);
                return false;
            }
        }

        if (sr_input_end(input) != SR_OK) {
            result.error = QStringLiteral("Failed to finish importing %1.").arg(fileName);
            return false;
        }

        return true;
    };

    sr_input *probe_input = make_input();
    if (!probe_input) {
        result.error = QStringLiteral("Failed to initialize import format %1.").arg(formatId);
        return result;
    }

    ds_set_datafeed_callback(discard_datafeed_callback);
    if (!send_file(probe_input)) {
        dsv_info("InputImporter: probe pass failed error=%s",
                 result.error.toUtf8().constData());
        sr_input_free(probe_input);
        ds_set_datafeed_callback(SigSession::data_feed_callback);
        return result;
    }
    dsv_info("InputImporter: probe pass done sdi_ready=%d channels=%u enabled=%u",
             probe_input->sdi_ready ? 1 : 0,
             channelCount(sr_input_dev_inst_get(probe_input)),
             enabledChannelCount(sr_input_dev_inst_get(probe_input)));

    if (!probe_input->sdi_ready) {
        sr_input_free(probe_input);
        ds_set_datafeed_callback(SigSession::data_feed_callback);
        result.error = QStringLiteral("Import format %1 did not produce a usable device.").arg(formatId);
        return result;
    }

    sr_dev_inst *ownedDevice = cloneImportedDevice(sr_input_dev_inst_get(probe_input));
    sr_input_free(probe_input);
    ds_set_datafeed_callback(SigSession::data_feed_callback);

    if (!ownedDevice) {
        result.error = QStringLiteral("Failed to preserve imported device state for %1.").arg(fileName);
        return result;
    }

    session.set_as_current();
    session.bind_imported_device(ownedDevice,
                                 inferWorkMode(ownedDevice, plan.workMode),
                                 clampSampleLimit(plan.sampleRate),
                                 clampSampleLimit(plan.sampleLimit),
                                 displayName.isEmpty() ? fileName : displayName,
                                 fileName);
    // Binding may initialize application-owned helpers that create their own
    // SigSession and replace the global libsigrok datafeed route.
    session.set_as_current();
    dsv_info("InputImporter: bound imported device session=%p channels=%u enabled=%u samplerate=%llu samplelimit=%llu",
             (void *)&session,
             channelCount(ownedDevice),
             enabledChannelCount(ownedDevice),
             (unsigned long long)plan.sampleRate,
             (unsigned long long)plan.sampleLimit);

    sr_input *real_input = make_input();
    if (!real_input) {
        result.error = QStringLiteral("Failed to initialize import format %1.").arg(formatId);
        return result;
    }

    if (!send_file(real_input)) {
        dsv_info("InputImporter: real pass failed error=%s",
                 result.error.toUtf8().constData());
        sr_input_free(real_input);
        return result;
    }
    dsv_info("InputImporter: real pass done sdi_ready=%d channels=%u enabled=%u",
             real_input->sdi_ready ? 1 : 0,
             channelCount(sr_input_dev_inst_get(real_input)),
             enabledChannelCount(sr_input_dev_inst_get(real_input)));

    sr_input_free(real_input);
    const uint64_t sample_limit_override =
        plan.preserveHeaderSampleLimit ? plan.sampleLimit : 0;
    if (sample_limit_override != 0) {
        dsv_info("InputImporter: finish imported capture samplelimit_override=%llu current=%llu",
                 (unsigned long long)sample_limit_override,
                 (unsigned long long)session.cur_samplelimits());
    }
    session.finish_imported_capture(sample_limit_override);
    result.ok = true;
    return result;
}

} // namespace data
} // namespace pv
