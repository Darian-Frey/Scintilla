#pragma once

#include <QWidget>
#include <array>
#include <cstdint>
#include <vector>

#include "core/VoxelFrame.h"     // RGB

class QSlider;
class QSpinBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QGridLayout;

// ── ColorPickerWidget ────────────────────────────────────────────────────────
//
// RGB picker (sliders + hex), 18-preset palette, 16-slot LIFO history.
// Emits colorChanged when the user changes the colour by any path.
// Pick tool feedback enters via setCurrentColor() (silently adds to history).

class ColorPickerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ColorPickerWidget(QWidget* parent = nullptr);

    [[nodiscard]] RGB currentColor() const { return m_color; }
    void setCurrentColor(uint8_t r, uint8_t g, uint8_t b);

signals:
    void colorChanged(uint8_t r, uint8_t g, uint8_t b);

private slots:
    void onSliderChanged();
    void onHexEdited();
    void onPaletteClicked(int index);
    void onHistoryClicked(int slot);

private:
    void buildLayout();
    void buildPalette(QGridLayout* into);
    void buildHistory(QGridLayout* into);
    void applyColorToUi();
    void emitChanged();
    void pushHistory(RGB color);

    RGB              m_color   = {255, 64, 32};  // base colour (pre-brightness)
    int              m_brightness = 100;          // 0..100 percent
    std::vector<RGB> m_history;

    QSlider*    m_rSlider      = nullptr;
    QSlider*    m_gSlider      = nullptr;
    QSlider*    m_bSlider      = nullptr;
    QSlider*    m_brightSlider = nullptr;
    QLabel*     m_rLabel       = nullptr;
    QLabel*     m_gLabel       = nullptr;
    QLabel*     m_bLabel       = nullptr;
    QSpinBox*   m_brightSpin   = nullptr;
    QLineEdit*  m_hexEdit      = nullptr;
    QLabel*     m_preview      = nullptr;
    std::vector<QPushButton*> m_paletteBtns;
    std::vector<QPushButton*> m_historyBtns;
};
