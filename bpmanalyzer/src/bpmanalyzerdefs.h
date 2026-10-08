/*
 * Fooyin BPM Analyzer Plugin
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

#pragma once

#include <core/coresettings.h>

#include <QString>

#include <algorithm>
#include <cmath>

namespace Fooyin::BpmAnalyzer {

// Standard tag field written to track metadata
constexpr auto BpmTagField = "BPM";

// Settings keys
constexpr auto SettingAggregationMethod    = "BpmAnalyzer/AggregationMethod";
constexpr auto SettingAnalysisSampleLength = "BpmAnalyzer/SampleLength";
constexpr auto SettingSkipExisting         = "BpmAnalyzer/SkipExisting";
constexpr auto SettingBpmPrecision         = "BpmAnalyzer/Precision";
constexpr auto SettingConcurrencyAuto      = "BpmAnalyzer/ConcurrencyAuto";
constexpr auto SettingConcurrencyCount     = "BpmAnalyzer/ConcurrencyCount";

// Defaults
constexpr int DefaultSampleLength      = 60;  // seconds
constexpr int DefaultConcurrencyCount  = 1;
constexpr int DefaultAggregationMethod = 0;   // WeightedAverage
constexpr int DefaultBpmPrecision      = 0;   // Integer

/*!
 * Specifies how multiple BPM candidates derived from the beat detector are
 * combined into a single reported BPM value.
 *
 * Candidates are computed as 60.0 / (inter-beat interval in seconds) from the
 * individual beat positions returned by SoundTouch BPMDetect::getBeats().
 */
enum class AggregationMethod : int
{
    WeightedAverage = 0,  ///< Σ(bpm_i × w_i) / Σ(w_i), weights = mean strength of adjacent beats
    Mean,                 ///< Simple arithmetic mean of all candidates
    Median,               ///< Sorted mid-point of all candidates
    Mode,                 ///< Integer-BPM histogram bin with highest total weight
};

//! Single source of truth for the plugin's persisted settings.
struct BpmAnalyzerSettings
{
    int sampleLength{DefaultSampleLength};
    bool skipExisting{false};
    int aggregationMethod{DefaultAggregationMethod};
    int bpmPrecision{DefaultBpmPrecision};
    bool concurrencyAuto{false};
    int concurrencyCount{DefaultConcurrencyCount};

    static BpmAnalyzerSettings load()
    {
        FySettings settings;
        BpmAnalyzerSettings result;
        result.sampleLength = std::clamp(
            settings.value(QLatin1String{SettingAnalysisSampleLength},
                           DefaultSampleLength).toInt(),
            1, 600);
        result.skipExisting = settings.value(QLatin1String{SettingSkipExisting}, false).toBool();
        result.aggregationMethod = settings.value(QLatin1String{SettingAggregationMethod},
                                                  DefaultAggregationMethod).toInt();
        result.bpmPrecision = settings.value(QLatin1String{SettingBpmPrecision},
                                             DefaultBpmPrecision).toInt();
        result.concurrencyAuto = settings.value(QLatin1String{SettingConcurrencyAuto}, false).toBool();
        result.concurrencyCount = settings.value(QLatin1String{SettingConcurrencyCount},
                                                 DefaultConcurrencyCount).toInt();
        return result;
    }

    void save() const
    {
        FySettings settings;
        settings.setValue(QLatin1String{SettingAnalysisSampleLength}, sampleLength);
        settings.setValue(QLatin1String{SettingSkipExisting}, skipExisting);
        settings.setValue(QLatin1String{SettingAggregationMethod}, aggregationMethod);
        settings.setValue(QLatin1String{SettingBpmPrecision}, bpmPrecision);
        settings.setValue(QLatin1String{SettingConcurrencyAuto}, concurrencyAuto);
        settings.setValue(QLatin1String{SettingConcurrencyCount}, concurrencyCount);
    }
};

//! Formats a BPM value with the configured precision (0, 1 or 2 decimals).
inline QString formatBpmValue(float bpm, int precision)
{
    switch(precision) {
        case 1:
            return QString::number(static_cast<double>(bpm), 'f', 1);
        case 2:
            return QString::number(static_cast<double>(bpm), 'f', 2);
        default:
            return QString::number(static_cast<int>(std::round(bpm)));
    }
}

} // namespace Fooyin::BpmAnalyzer
