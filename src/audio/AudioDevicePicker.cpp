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
        tr("To capture audio playing on this computer, choose one of the "
           "[system audio] monitor sources at the top. Selecting one tells "
           "Scintilla to redirect the system default input to that monitor "
           "until you change it (or reboot). To capture from a real "
           "microphone instead, pick a regular device below."),
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
    m_devices  = AudioReactiveEngine::enumerateDevices();
    m_monitors = AudioReactiveEngine::enumerateMonitors();

    // ─── Monitor sources first (system-audio capture) ───────────────────────
    if (!m_monitors.isEmpty()) {
        auto* header = new QListWidgetItem(
            tr("── System audio (PulseAudio / PipeWire monitor sources) ──"),
            m_list);
        header->setFlags(Qt::NoItemFlags);
        QFont hf = header->font();
        hf.setItalic(true);
        header->setFont(hf);

        for (const auto& m : m_monitors) {
            auto* it = new QListWidgetItem(
                tr("[system audio] %1").arg(m.description), m_list);
            // UserRole = -1 sentinel meaning "this is a monitor pick"; the
            // monitor name lives in UserRole+1.
            it->setData(Qt::UserRole,     -1);
            it->setData(Qt::UserRole + 1, m.name);
            QFont f = it->font();
            f.setBold(true);
            it->setFont(f);
        }

        // Divider before PortAudio devices
        auto* divider = new QListWidgetItem(
            tr("── PortAudio capture devices ──"), m_list);
        divider->setFlags(Qt::NoItemFlags);
        divider->setFont(hf);
    }

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

    auto* picked = items.first();
    const int idx = picked->data(Qt::UserRole).toInt();

    // Monitor-source picks have UserRole == -1 and the source name in +1.
    if (idx == -1) {
        const QString monitor = picked->data(Qt::UserRole + 1).toString();
        if (monitor.isEmpty()) {
            m_okButton->setEnabled(false);
            return;
        }
        m_okButton->setEnabled(true);
        m_detailLabel->setText(
            tr("System audio (monitor source)\n"
               "Name: %1\n"
               "Selecting this redirects the system default input to this "
               "monitor via `pactl set-default-source` before opening "
               "PortAudio. The change persists until you set it back."
              ).arg(monitor));
        // Sample rate combo is populated against the resolved default device
        // — the actual PortAudio default index is queried later.
        const int defaultIdx = m_devices.isEmpty() ? -1 : m_devices.first().index;
        refreshSampleRates(defaultIdx);
        return;
    }

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

    auto* picked = items.first();
    const int idx = picked->data(Qt::UserRole).toInt();

    if (idx == -1) {
        // Monitor source pick. The engine's worker will resolve the
        // PortAudio default device index after pactl runs.
        m_monitorSource = picked->data(Qt::UserRole + 1).toString();
        if (m_monitorSource.isEmpty()) return;
        m_deviceIndex   = -1;
    } else {
        if (idx < 0) return;
        m_deviceIndex   = idx;
        m_monitorSource.clear();
    }

    const QVariant rateData = m_rateCombo->currentData();
    if (rateData.isValid()) m_sampleRate = static_cast<float>(rateData.toInt());

    accept();
}

void AudioDevicePicker::onRefresh() {
    populate();
}
