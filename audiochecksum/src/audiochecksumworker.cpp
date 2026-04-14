/*
 * Fooyin AudioChecksum Plugin
 * Copyright © 2026
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "audiochecksumworker.h"

#include "audiochecksumdefs.h"
#include "flacstreaminfo.h"

#include <core/engine/audiobuffer.h>
#include <core/engine/audioformat.h>
#include <core/engine/audioloader.h>
#include <core/engine/audioinput.h>

#include <QCryptographicHash>
#include <QFileInfo>

#include <algorithm>
#include <bit>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>

extern "C"
{
#include <libavcodec/version.h>
#include <libavutil/channel_layout.h>
#include <libavutil/mathematics.h>
#include <libswresample/swresample.h>
}

using namespace Qt::StringLiterals;

#define OLD_CHANNEL_LAYOUT (LIBAVCODEC_VERSION_INT < AV_VERSION_INT(59, 24, 100))

namespace Fooyin::AudioChecksum {

namespace {

bool isFlacTrack(const Track& track)
{
    const QString codec = track.codec().toLower();
    return codec == u"flac"_s
        || track.filepath().endsWith(u".flac"_s, Qt::CaseInsensitive);
}

uint64_t channelBit(const AudioFormat::ChannelPosition position)
{
    using P = AudioFormat::ChannelPosition;
    switch(position) {
        case P::FrontLeft:
            return static_cast<uint64_t>(AV_CH_FRONT_LEFT);
        case P::FrontRight:
            return static_cast<uint64_t>(AV_CH_FRONT_RIGHT);
        case P::FrontCenter:
            return static_cast<uint64_t>(AV_CH_FRONT_CENTER);
        case P::LFE:
            return static_cast<uint64_t>(AV_CH_LOW_FREQUENCY);
        case P::BackLeft:
            return static_cast<uint64_t>(AV_CH_BACK_LEFT);
        case P::BackRight:
            return static_cast<uint64_t>(AV_CH_BACK_RIGHT);
        case P::FrontLeftOfCenter:
            return static_cast<uint64_t>(AV_CH_FRONT_LEFT_OF_CENTER);
        case P::FrontRightOfCenter:
            return static_cast<uint64_t>(AV_CH_FRONT_RIGHT_OF_CENTER);
        case P::BackCenter:
            return static_cast<uint64_t>(AV_CH_BACK_CENTER);
        case P::SideLeft:
            return static_cast<uint64_t>(AV_CH_SIDE_LEFT);
        case P::SideRight:
            return static_cast<uint64_t>(AV_CH_SIDE_RIGHT);
        case P::TopCenter:
            return static_cast<uint64_t>(AV_CH_TOP_CENTER);
        case P::TopFrontLeft:
            return static_cast<uint64_t>(AV_CH_TOP_FRONT_LEFT);
        case P::TopFrontCenter:
            return static_cast<uint64_t>(AV_CH_TOP_FRONT_CENTER);
        case P::TopFrontRight:
            return static_cast<uint64_t>(AV_CH_TOP_FRONT_RIGHT);
        case P::TopBackLeft:
            return static_cast<uint64_t>(AV_CH_TOP_BACK_LEFT);
        case P::TopBackCenter:
            return static_cast<uint64_t>(AV_CH_TOP_BACK_CENTER);
        case P::TopBackRight:
            return static_cast<uint64_t>(AV_CH_TOP_BACK_RIGHT);
#ifdef AV_CH_LOW_FREQUENCY_2
        case P::LFE2:
            return static_cast<uint64_t>(AV_CH_LOW_FREQUENCY_2);
#endif
#ifdef AV_CH_TOP_SIDE_LEFT
        case P::TopSideLeft:
            return static_cast<uint64_t>(AV_CH_TOP_SIDE_LEFT);
#endif
#ifdef AV_CH_TOP_SIDE_RIGHT
        case P::TopSideRight:
            return static_cast<uint64_t>(AV_CH_TOP_SIDE_RIGHT);
#endif
#ifdef AV_CH_BOTTOM_FRONT_CENTER
        case P::BottomFrontCenter:
            return static_cast<uint64_t>(AV_CH_BOTTOM_FRONT_CENTER);
#endif
#ifdef AV_CH_BOTTOM_FRONT_LEFT
        case P::BottomFrontLeft:
            return static_cast<uint64_t>(AV_CH_BOTTOM_FRONT_LEFT);
#endif
#ifdef AV_CH_BOTTOM_FRONT_RIGHT
        case P::BottomFrontRight:
            return static_cast<uint64_t>(AV_CH_BOTTOM_FRONT_RIGHT);
#endif
        case P::UnknownPosition:
        default:
            return 0;
    }
}

std::optional<uint64_t> explicitMaskFromLayout(const AudioFormat& format)
{
    if(!format.hasChannelLayout())
        return {};

    const int channelCount = std::max(1, format.channelCount());
    const auto layout      = format.channelLayoutView();
    if(std::cmp_less(layout.size(), channelCount))
        return {};

    uint64_t mask{0};
    for(int i{0}; i < channelCount; ++i) {
        const uint64_t bit = channelBit(layout[static_cast<size_t>(i)]);
        if(bit == 0 || (mask & bit) != 0)
            return {};
        mask |= bit;
    }

    if(std::popcount(mask) != channelCount)
        return {};

    return mask;
}

#if OLD_CHANNEL_LAYOUT
uint64_t channelMaskForFormat(const AudioFormat& format)
{
    if(const auto explicitMask = explicitMaskFromLayout(format))
        return *explicitMask;

    const int channelCount   = std::max(1, format.channelCount());
    const int64_t defaultMsk = av_get_default_channel_layout(channelCount);
    if(defaultMsk > 0)
        return static_cast<uint64_t>(defaultMsk);

    return 0;
}
#else
bool channelLayoutForFormat(const AudioFormat& format, AVChannelLayout& layout)
{
    layout = {};

    const int channelCount = std::max(1, format.channelCount());
    if(const auto explicitMask = explicitMaskFromLayout(format)) {
        if(av_channel_layout_from_mask(&layout, *explicitMask) == 0
           && layout.nb_channels == channelCount) {
            return true;
        }
        av_channel_layout_uninit(&layout);
    }

    av_channel_layout_default(&layout, channelCount);
    return layout.nb_channels == channelCount;
}
#endif

AVSampleFormat avSampleFormatFor(const AudioFormat& format)
{
    switch(format.sampleFormat()) {
        case SampleFormat::U8:
            return AV_SAMPLE_FMT_U8;
        case SampleFormat::S16:
            return AV_SAMPLE_FMT_S16;
        case SampleFormat::S24In32:
        case SampleFormat::S32:
            return AV_SAMPLE_FMT_S32;
        case SampleFormat::F32:
            return AV_SAMPLE_FMT_FLT;
        case SampleFormat::F64:
            return AV_SAMPLE_FMT_DBL;
        default:
            return AV_SAMPLE_FMT_NONE;
    }
}

struct SwrContextDeleter
{
    void operator()(SwrContext* context) const
    {
        if(context)
            swr_free(&context);
    }
};

using SwrContextPtr = std::unique_ptr<SwrContext, SwrContextDeleter>;

SwrContextPtr createFfmpegMd5Converter(const AudioFormat& format)
{
    SwrContext* context = nullptr;
    const AVSampleFormat inputFmt = avSampleFormatFor(format);
    if(inputFmt == AV_SAMPLE_FMT_NONE)
        return {};

#if OLD_CHANNEL_LAYOUT
    const uint64_t channelMask = channelMaskForFormat(format);
    if(channelMask == 0)
        return {};

    context = swr_alloc_set_opts(nullptr,
                                 static_cast<int64_t>(channelMask), AV_SAMPLE_FMT_S16,
                                 format.sampleRate(),
                                 static_cast<int64_t>(channelMask), inputFmt,
                                 format.sampleRate(), 0, nullptr);
    if(!context)
        return {};
#else
    AVChannelLayout inLayout{};
    AVChannelLayout outLayout{};
    if(!channelLayoutForFormat(format, inLayout)
       || !channelLayoutForFormat(format, outLayout)) {
        av_channel_layout_uninit(&inLayout);
        av_channel_layout_uninit(&outLayout);
        return {};
    }

    const int rc = swr_alloc_set_opts2(&context,
                                       &outLayout, AV_SAMPLE_FMT_S16, format.sampleRate(),
                                       &inLayout, inputFmt, format.sampleRate(),
                                       0, nullptr);
    av_channel_layout_uninit(&inLayout);
    av_channel_layout_uninit(&outLayout);
    if(rc < 0 || !context) {
        swr_free(&context);
        return {};
    }
#endif

    if(swr_init(context) < 0) {
        swr_free(&context);
        return {};
    }

    return SwrContextPtr{context};
}

int estimateMaxOutFrames(SwrContext* context, int inFrames, int inputRate, int outputRate)
{
    const int safeInFrames   = std::max(0, inFrames);
    const int safeInputRate  = std::max(1, inputRate);
    const int safeOutputRate = std::max(1, outputRate);

    const int suggested = swr_get_out_samples(context, safeInFrames);
    if(suggested > 0)
        return suggested;

    const int64_t delay = std::max<int64_t>(0, swr_get_delay(context, safeInputRate));
    const int64_t scaled
        = av_rescale_rnd(delay + static_cast<int64_t>(safeInFrames),
                         safeOutputRate, safeInputRate, AV_ROUND_UP);
    return static_cast<int>(std::clamp<int64_t>(scaled + 32, 1, std::numeric_limits<int>::max()));
}

bool addFfmpegMd5HashData(QCryptographicHash& hash, SwrContext* converter,
                          const AudioBuffer& buffer)
{
    const AudioFormat format = buffer.format();
    const int inFrames       = buffer.frameCount();
    const int channelCount   = std::max(1, format.channelCount());
    if(inFrames <= 0)
        return true;

    const int maxOutFrames = estimateMaxOutFrames(converter, inFrames,
                                                  format.sampleRate(), format.sampleRate());
    QByteArray converted(maxOutFrames * channelCount * static_cast<int>(sizeof(qint16)),
                         Qt::Uninitialized);

    const uint8_t* inPlanes[1]{reinterpret_cast<const uint8_t*>(buffer.data())};
    uint8_t* outPlanes[1]{reinterpret_cast<uint8_t*>(converted.data())};

    const int outFrames = swr_convert(converter, outPlanes, maxOutFrames, inPlanes, inFrames);
    if(outFrames < 0)
        return false;

    converted.resize(outFrames * channelCount * static_cast<int>(sizeof(qint16)));
    hash.addData(converted);
    return true;
}

bool flushFfmpegMd5HashData(QCryptographicHash& hash, SwrContext* converter,
                            const AudioFormat& format)
{
    const int channelCount = std::max(1, format.channelCount());
    for(;;) {
        const int maxOutFrames = std::max(1,
            estimateMaxOutFrames(converter, 0, format.sampleRate(), format.sampleRate()));
        QByteArray converted(maxOutFrames * channelCount * static_cast<int>(sizeof(qint16)),
                             Qt::Uninitialized);
        uint8_t* outPlanes[1]{reinterpret_cast<uint8_t*>(converted.data())};

        const int outFrames = swr_convert(converter, outPlanes, maxOutFrames, nullptr, 0);
        if(outFrames < 0)
            return false;
        if(outFrames == 0)
            return true;

        converted.resize(outFrames * channelCount * static_cast<int>(sizeof(qint16)));
        hash.addData(converted);
    }
}

bool addHashData(QCryptographicHash& hash, const AudioBuffer& buffer,
                 bool useFlacCanonicalMd5, SwrContext* ffmpegMd5Converter)
{
    if(useFlacCanonicalMd5) {
        if(buffer.format().sampleFormat() == SampleFormat::S24In32) {
            // FFmpeg decodes 24-bit FLAC into AV_SAMPLE_FMT_S32 with samples
            // left-aligned (shifted << 8), so in LE memory each 4-byte word is
            // [0x00, LSB, MID, MSB]. The FLAC STREAMINFO MD5 is computed over
            // tightly-packed 3 bytes/sample LE data [LSB, MID, MSB], so we must
            // skip the low zero byte and copy only bytes [1,2,3] of each word.
            const auto* src = reinterpret_cast<const char*>(buffer.data());
            const int total = static_cast<int>(buffer.byteCount());
            const int count = total / 4;
            QByteArray packed(count * 3, Qt::Uninitialized);
            char* dst = packed.data();
            for(int i = 0; i < count; ++i) {
                std::memcpy(dst + i * 3, src + i * 4 + 1, 3);
            }
            hash.addData(packed);
            return true;
        }

        hash.addData(QByteArrayView{reinterpret_cast<const char*>(buffer.data()),
                                    static_cast<qsizetype>(buffer.byteCount())});
        return true;
    }

    return ffmpegMd5Converter != nullptr
        && addFfmpegMd5HashData(hash, ffmpegMd5Converter, buffer);
}

} // namespace

AudioChecksumWorker::AudioChecksumWorker(std::shared_ptr<AudioLoader> audioLoader,
                                         QObject* parent)
    : QObject{parent}
    , m_audioLoader{std::move(audioLoader)}
{ }

ChecksumResult AudioChecksumWorker::computeChecksum(const Track& track,
                                                     const QAtomicInt& cancelled) const
{
    ChecksumResult result;
    result.track = track;

    const bool useFlacCanonicalMd5 = isFlacTrack(track);
    result.algorithm = useFlacCanonicalMd5 ? u"MD5 (FLAC)"_s
                                           : u"MD5 (FFmpeg pcm_s16le)"_s;

    // Retrieve any stored tag from the track
    const QStringList storedValues = track.extraTag(tagFieldName());
    if(!storedValues.isEmpty())
        result.storedHash = storedValues.first().toLower();

    // For FLAC: also extract the embedded STREAMINFO MD5 as a reference.
    // Overwrite storedHash only when no tag was set manually, so user-provided
    // tags take precedence over the encoder-embedded value.
    if(useFlacCanonicalMd5) {
        const QString flacMd5 = readFlacStreamInfoMd5(track.filepath());
        if(!flacMd5.isEmpty() && result.storedHash.isEmpty())
            result.storedHash = flacMd5;
    }

    // Decode and hash
    const auto loaded = m_audioLoader->loadDecoderForTrack(
        track,
        AudioDecoder::NoSeeking | AudioDecoder::NoInfiniteLooping);

    if(!loaded.decoder) {
        result.status      = ChecksumResult::Status::Error;
        result.errorString = QObject::tr("No decoder available");
        return result;
    }

    const AudioFormat fmt = loaded.format.value_or(AudioFormat{});
    if(!fmt.isValid()) {
        result.status      = ChecksumResult::Status::Error;
        result.errorString = QObject::tr("Could not determine audio format");
        return result;
    }

    loaded.decoder->start();

    QCryptographicHash hash{QCryptographicHash::Md5};
    SwrContextPtr ffmpegMd5Converter;
    if(!useFlacCanonicalMd5) {
        ffmpegMd5Converter = createFfmpegMd5Converter(fmt);
        if(!ffmpegMd5Converter) {
            loaded.decoder->stop();
            result.status      = ChecksumResult::Status::Error;
            result.errorString = QObject::tr("Could not initialise FFmpeg PCM converter");
            return result;
        }
    }

    constexpr size_t ChunkBytes = 65536;
    while(!cancelled.loadRelaxed()) {
        AudioBuffer buffer = loaded.decoder->readBuffer(ChunkBytes);
        if(!buffer.isValid() || buffer.byteCount() == 0)
            break;

        if(!addHashData(hash, buffer, useFlacCanonicalMd5, ffmpegMd5Converter.get())) {
            loaded.decoder->stop();
            result.status      = ChecksumResult::Status::Error;
            result.errorString = QObject::tr("Could not convert decoded audio to FFmpeg PCM");
            return result;
        }
    }

    if(!cancelled.loadRelaxed() && ffmpegMd5Converter
       && !flushFfmpegMd5HashData(hash, ffmpegMd5Converter.get(), fmt)) {
        loaded.decoder->stop();
        result.status      = ChecksumResult::Status::Error;
        result.errorString = QObject::tr("Could not flush FFmpeg PCM converter");
        return result;
    }

    loaded.decoder->stop();

    if(cancelled.loadRelaxed()) {
        // Cancelled — return a partial/empty result rather than a wrong hash
        result.status      = ChecksumResult::Status::Error;
        result.errorString = QObject::tr("Cancelled");
        return result;
    }

    result.computedHash = QString::fromLatin1(hash.result().toHex());

    if(!result.storedHash.isEmpty()) {
        result.status = (result.computedHash == result.storedHash) ? ChecksumResult::Status::Match
                                                                   : ChecksumResult::Status::Mismatch;
    }
    // else: storedHash is empty — status stays at its default (New)

    return result;
}

} // namespace Fooyin::AudioChecksum

#include "moc_audiochecksumworker.cpp"
