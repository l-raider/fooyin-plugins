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

#include "bpmanalyzerplugin_settings.h"

#include "bpmanalyzerdefs.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSlider>
#include <QSpinBox>
#include <QThread>
#include <QVBoxLayout>

using namespace Qt::StringLiterals;

namespace Fooyin::BpmAnalyzer {

BpmAnalyzerSettingsDialog::BpmAnalyzerSettingsDialog(QWidget* parent)
    : QDialog{parent}
    , m_sampleLength{new QSpinBox(this)}
    , m_skipExisting{new QCheckBox(tr("Skip tracks that already have a BPM tag"), this)}
    , m_aggregationMethod{new QComboBox(this)}
    , m_bpmPrecision{new QComboBox(this)}
    , m_autoConcurrency{new QCheckBox(tr("Auto (use all CPU cores)"), this)}
    , m_concurrencySlider{new QSlider(Qt::Horizontal, this)}
    , m_concurrencyValueLabel{new QLabel(this)}
{
    setWindowTitle(tr("BPM Analyzer Settings"));
    setModal(true);

    // ---- Analysis group ----
    m_sampleLength->setRange(1, 600);
    m_sampleLength->setSuffix(tr(" s"));
    m_sampleLength->setValue(
        m_settings.value(QLatin1String{SettingAnalysisSampleLength},
                         DefaultSampleLength).toInt());

    m_skipExisting->setChecked(
        m_settings.value(QLatin1String{SettingSkipExisting}, false).toBool());

    auto* sampleLengthLabel = new QLabel(tr("Analysis length:"), this);

    auto* analysisLayout = new QHBoxLayout;
    analysisLayout->addWidget(sampleLengthLabel);
    analysisLayout->addWidget(m_sampleLength);
    analysisLayout->addStretch();

    auto* analysisGroup  = new QGroupBox(tr("Analysis"), this);
    auto* analysisVLayout = new QVBoxLayout(analysisGroup);
    analysisVLayout->addLayout(analysisLayout);
    analysisVLayout->addWidget(m_skipExisting);

    // ---- Aggregation group ----
    m_aggregationMethod->addItem(tr("Weighted Average (recommended)"),
                                 static_cast<int>(AggregationMethod::WeightedAverage));
    m_aggregationMethod->addItem(tr("Mean (arithmetic average)"),
                                 static_cast<int>(AggregationMethod::Mean));
    m_aggregationMethod->addItem(tr("Median"),
                                 static_cast<int>(AggregationMethod::Median));
    m_aggregationMethod->addItem(tr("Mode (most frequent integer BPM)"),
                                 static_cast<int>(AggregationMethod::Mode));

    {
        const int savedMethod =
            m_settings.value(QLatin1String{SettingAggregationMethod},
                             DefaultAggregationMethod).toInt();
        const int idx = m_aggregationMethod->findData(savedMethod);
        m_aggregationMethod->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    auto* aggLabel  = new QLabel(tr("Aggregation method:"), this);
    auto* aggLayout = new QHBoxLayout;
    aggLayout->addWidget(aggLabel);
    aggLayout->addWidget(m_aggregationMethod, 1);

    auto* aggGroup  = new QGroupBox(tr("BPM Aggregation"), this);
    auto* aggVLayout = new QVBoxLayout(aggGroup);
    aggVLayout->addLayout(aggLayout);

    // ---- Output group ----
    m_bpmPrecision->addItem(tr("Integer (e.g. 120)"),  0);
    m_bpmPrecision->addItem(tr("1 decimal (e.g. 120.5)"), 1);
    m_bpmPrecision->addItem(tr("2 decimals (e.g. 120.53)"), 2);

    {
        const int savedPrec =
            m_settings.value(QLatin1String{SettingBpmPrecision},
                             DefaultBpmPrecision).toInt();
        const int idx = m_bpmPrecision->findData(savedPrec);
        m_bpmPrecision->setCurrentIndex(idx >= 0 ? idx : 0);
    }

    auto* precLabel  = new QLabel(tr("BPM precision:"), this);
    auto* precLayout = new QHBoxLayout;
    precLayout->addWidget(precLabel);
    precLayout->addWidget(m_bpmPrecision, 1);

    auto* outputGroup   = new QGroupBox(tr("Output"), this);
    auto* outputVLayout = new QVBoxLayout(outputGroup);
    outputVLayout->addLayout(precLayout);

    // ---- Concurrency group ----
    const int maxThreads = std::max(1, QThread::idealThreadCount());

    m_concurrencySlider->setRange(1, maxThreads);
    m_concurrencySlider->setTickPosition(QSlider::TicksBelow);
    m_concurrencySlider->setTickInterval(1);
    m_concurrencySlider->setSingleStep(1);
    m_concurrencySlider->setPageStep(1);

    const bool isAuto =
        m_settings.value(QLatin1String{SettingConcurrencyAuto}, false).toBool();
    const int savedCount =
        m_settings.value(QLatin1String{SettingConcurrencyCount},
                         DefaultConcurrencyCount).toInt();
    m_autoConcurrency->setChecked(isAuto);
    m_concurrencySlider->setValue(std::clamp(savedCount, 1, maxThreads));
    m_concurrencySlider->setEnabled(!isAuto);

    const auto updateValueLabel = [this, maxThreads]() {
        m_concurrencyValueLabel->setText(
            QString::number(m_concurrencySlider->value()) + " / "_L1 +
            QString::number(maxThreads));
    };
    updateValueLabel();

    QObject::connect(m_concurrencySlider, &QSlider::valueChanged,
                     this, updateValueLabel);
    QObject::connect(m_autoConcurrency, &QCheckBox::toggled,
                     this, [this, updateValueLabel](bool checked) {
                         m_concurrencySlider->setEnabled(!checked);
                         updateValueLabel();
                     });

    auto* threadsLabel = new QLabel(tr("Threads:"), this);
    auto* sliderRow    = new QHBoxLayout;
    sliderRow->addWidget(threadsLabel);
    sliderRow->addWidget(m_concurrencySlider, 1);
    sliderRow->addWidget(m_concurrencyValueLabel);

    auto* concurrencyGroup  = new QGroupBox(tr("Concurrency"), this);
    auto* concurrencyLayout = new QVBoxLayout(concurrencyGroup);
    concurrencyLayout->addWidget(m_autoConcurrency);
    concurrencyLayout->addLayout(sliderRow);

    // ---- Buttons ----
    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    QObject::connect(buttons, &QDialogButtonBox::accepted,
                     this, &BpmAnalyzerSettingsDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected,
                     this, &BpmAnalyzerSettingsDialog::reject);

    // ---- Main layout ----
    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->addWidget(analysisGroup);
    mainLayout->addWidget(aggGroup);
    mainLayout->addWidget(outputGroup);
    mainLayout->addWidget(concurrencyGroup);
    mainLayout->addStretch();
    mainLayout->addWidget(buttons);
}

void BpmAnalyzerSettingsDialog::accept()
{
    m_settings.setValue(QLatin1String{SettingAnalysisSampleLength},
                        m_sampleLength->value());
    m_settings.setValue(QLatin1String{SettingSkipExisting},
                        m_skipExisting->isChecked());
    m_settings.setValue(QLatin1String{SettingAggregationMethod},
                        m_aggregationMethod->currentData().toInt());
    m_settings.setValue(QLatin1String{SettingBpmPrecision},
                        m_bpmPrecision->currentData().toInt());
    m_settings.setValue(QLatin1String{SettingConcurrencyAuto},
                        m_autoConcurrency->isChecked());
    m_settings.setValue(QLatin1String{SettingConcurrencyCount},
                        m_concurrencySlider->value());

    done(Accepted);
}

} // namespace Fooyin::BpmAnalyzer
