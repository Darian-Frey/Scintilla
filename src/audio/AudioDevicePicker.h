#pragma once

#include <QDialog>
#include "AudioReactiveEngine.h"   // for DeviceInfo

class QListWidget;
class QLabel;
class QPushButton;
class QComboBox;

// ── AudioDevicePicker ────────────────────────────────────────────────────────
//
// Modal dialog for selecting a PortAudio input device. Lists every enumerable
// input device with a "(monitor)" tag where heuristically a system-output
// loopback (DEC-009).
//
// Usage:
//   AudioDevicePicker dlg(this);
//   if (dlg.exec() == QDialog::Accepted) {
//       int idx = dlg.selectedDeviceIndex();
//       float sr = dlg.selectedSampleRate();
//       engine->start(idx, sr);
//   }
//
// The chosen device index is the PortAudio host-API device index, as used
// by Pa_OpenStream(). The selected sample rate defaults to 44100 Hz but
// adapts to the device's preferred default.

class AudioDevicePicker : public QDialog {
    Q_OBJECT

public:
    explicit AudioDevicePicker(QWidget* parent = nullptr);

    [[nodiscard]] int     selectedDeviceIndex()  const { return m_deviceIndex; }
    [[nodiscard]] float   selectedSampleRate()   const { return m_sampleRate; }
    // Non-empty iff the user picked a PulseAudio / PipeWire monitor source.
    // When set, the engine should run pactl set-default-source on this name
    // before opening PortAudio's "default" device.
    [[nodiscard]] QString selectedMonitorSource() const { return m_monitorSource; }

private slots:
    void onSelectionChanged();
    void onAccept();
    void onRefresh();

private:
    void buildLayout();
    void populate();
    void refreshSampleRates(int deviceIndex);

    QListWidget*  m_list           = nullptr;
    QLabel*       m_detailLabel    = nullptr;
    QComboBox*    m_rateCombo      = nullptr;
    QPushButton*  m_okButton       = nullptr;
    QPushButton*  m_refreshButton  = nullptr;

    QList<AudioReactiveEngine::DeviceInfo>    m_devices;
    QList<AudioReactiveEngine::MonitorSource> m_monitors;
    int     m_deviceIndex   = -1;
    float   m_sampleRate    = 44100.0f;
    QString m_monitorSource;     // set when the user picks a monitor entry
};
