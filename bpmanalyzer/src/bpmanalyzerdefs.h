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

} // namespace Fooyin::BpmAnalyzer
