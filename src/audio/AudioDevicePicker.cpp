#include "AudioDevicePicker.h"

#include <QComboBox>
#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QPushButton>
#include <QVBoxLayout>
#include <cmath>

AudioDevicePicker::AudioDevicePicker(QWidget* parent) : QDialog(parent) {
    setWindowTitle(tr("Select audio input"));
    resize(560, 380);
    buildLayout();
    populate();
}

void AudioDevicePicker::buildLayout() {
    auto* root = new QVBoxLayout(this);

    root->addWidget(new QLabel(
        tr("Choose a system audio capture device. Entries tagged "
           "[recommended] route through a sound server (PipeWire / PulseAudio "
           "/ JACK) and are the most reliable picks for music capture on "
           "Linux. Hardware ALSA devices below them only capture if they "
           "have a real microphone or line-in attached."),
        this));

    m_list = new QListWidget(this);
    connect(m_list, &QListWidget::itemSelectionChanged,
            this, &AudioDevicePicker::onSelectionChanged);
    connect(m_list, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem*) { onAccept(); });
    root->addWidget(m_list, /*stretch*/ 1);

    m_detailLabel = new QLabel(tr("No device selected."), this);
    m_detailLabel->setWordWrap(true);
    root->addWidget(m_detailLabel);

    // Sample-rate row — populated when a device is selected.
    auto* rateRow = new QHBoxLayout();
    rateRow->addWidget(new QLabel(tr("Sample rate:"), this));
    m_rateCombo = new QComboBox(this);
    m_rateCombo->setEnabled(false);
    m_rateCombo->setMinimumWidth(160);
    rateRow->addWidget(m_rateCombo);
    rateRow->addStretch(1);
    root->addLayout(rateRow);

    auto* buttonRow = new QHBoxLayout();
    m_refreshButton = new QPushButton(tr("Refresh"), this);
    connect(m_refreshButton, &QPushButton::clicked, this, &AudioDevicePicker::onRefresh);
    buttonRow->addWidget(m_refreshButton);
    buttonRow->addStretch(1);

    auto* box = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    m_okButton = box->button(QDialogButtonBox::Ok);
    m_okButton->setEnabled(false);
    connect(box, &QDialogButtonBox::accepted, this, &AudioDevicePicker::onAccept);
    connect(box, &QDialogButtonBox::rejected, this, &AudioDevicePicker::reject);
    buttonRow->addWidget(box);

    root->addLayout(buttonRow);
}

void AudioDevicePicker::populate() {
    m_list->clear();
    m_devices = AudioReactiveEngine::enumerateDevices();

    for (const auto& d : m_devices) {
        // Build "[tag] name  —  host api  (extra)" so the user can see at a
        // glance which devices are likely to work and via which audio backend.
        QStringList parts;
        if (d.recommended) parts << tr("[recommended]");
        parts << d.name;
        if (!d.hostApi.isEmpty()) parts << QStringLiteral("—") << d.hostApi;
        if (d.isMonitor)         parts << tr("(monitor / loopback)");

        auto* it = new QListWidgetItem(parts.join(QChar(' ')), m_list);
        it->setData(Qt::UserRole, d.index);

        // Bold the recommended rows so they stand out in the scroll list.
        if (d.recommended) {
            QFont f = it->font();
            f.setBold(true);
            it->setFont(f);
        }
    }
    if (m_devices.isEmpty()) {
        m_list->addItem(tr("(no input devices found — install / start a sound server)"));
    }
}

void AudioDevicePicker::onSelectionChanged() {
    const auto items = m_list->selectedItems();
    if (items.isEmpty()) {
        m_okButton->setEnabled(false);
        m_rateCombo->setEnabled(false);
        m_rateCombo->clear();
        m_detailLabel->setText(tr("No device selected."));
        return;
    }
    const int idx = items.first()->data(Qt::UserRole).toInt();
    if (idx < 0) {
        m_okButton->setEnabled(false);
        m_rateCombo->setEnabled(false);
        m_rateCombo->clear();
        return;
    }
    m_okButton->setEnabled(true);

    // Find the matching DeviceInfo for the detail label.
    for (const auto& d : m_devices) {
        if (d.index == idx) {
            m_detailLabel->setText(
                tr("Index: %1\nName: %2\nHost API: %3\nMonitor: %4\n"
                   "Recommended: %5\nDefault rate: %6 Hz")
                    .arg(d.index)
                    .arg(d.name)
                    .arg(d.hostApi.isEmpty() ? tr("(unknown)") : d.hostApi)
                    .arg(d.isMonitor   ? tr("yes") : tr("no"))
                    .arg(d.recommended ? tr("yes") : tr("no"))
                    .arg(d.defaultSampleRate));
            break;
        }
    }

    refreshSampleRates(idx);
}

void AudioDevicePicker::refreshSampleRates(int deviceIndex) {
    QSignalBlocker block(m_rateCombo);
    m_rateCombo->clear();

    const QList<int> rates = AudioReactiveEngine::supportedSampleRates(deviceIndex);
    if (rates.isEmpty()) {
        m_rateCombo->addItem(tr("(no rates probed; will use 44100)"), 44100);
        m_rateCombo->setEnabled(false);
        return;
    }

    // Pick the device's default rate (rounded to the nearest probed rate) as
    // the initial selection — typically 48000 on PipeWire, 44100 on most
    // sound cards. Falls back to the first probed rate if none match.
    double defaultRate = 44100.0;
    for (const auto& d : m_devices) {
        if (d.index == deviceIndex) { defaultRate = d.defaultSampleRate; break; }
    }

    int bestIndex = 0;
    double bestDelta = std::numeric_limits<double>::infinity();
    for (int i = 0; i < rates.size(); ++i) {
        const int rate = rates[i];
        m_rateCombo->addItem(QStringLiteral("%1 Hz").arg(rate), rate);
        const double delta = std::abs(static_cast<double>(rate) - defaultRate);
        if (delta < bestDelta) { bestDelta = delta; bestIndex = i; }
    }
    m_rateCombo->setCurrentIndex(bestIndex);
    m_rateCombo->setEnabled(true);
}

void AudioDevicePicker::onAccept() {
    const auto items = m_list->selectedItems();
    if (items.isEmpty()) return;
    m_deviceIndex = items.first()->data(Qt::UserRole).toInt();
    if (m_deviceIndex < 0) return;

    // Pull the sample rate from the combo — the user may have changed it.
    const QVariant rateData = m_rateCombo->currentData();
    if (rateData.isValid()) m_sampleRate = static_cast<float>(rateData.toInt());

    accept();
}

void AudioDevicePicker::onRefresh() {
    populate();
}
