#pragma once

#include <QWidget>

class QSlider;
class QLabel;
class QPushButton;

// ── SliceControlWidget ───────────────────────────────────────────────────────
//
// Three independent sliders for the X, Y, Z slice indices (DEC-004 / F-008).
// Each slider ranges from −1 (show all) to gridSize − 1 (show only layer N).
// Emits sliceChanged whenever any of the three values changes.

class SliceControlWidget : public QWidget {
    Q_OBJECT

public:
    explicit SliceControlWidget(QWidget* parent = nullptr);

    // Update slider ranges when the grid size changes.
    // Existing slice values are preserved (clamped if out of range).
    void setGridSize(int n);

    [[nodiscard]] int sliceX() const;
    [[nodiscard]] int sliceY() const;
    [[nodiscard]] int sliceZ() const;

    void resetAll();   // set all three sliders to −1

signals:
    void sliceChanged(int sliceX, int sliceY, int sliceZ);

private slots:
    void onSliderChanged();

private:
    void buildLayout();
    void refreshLabels();

    int m_gridSize = 8;

    QSlider* m_sliderX = nullptr;
    QSlider* m_sliderY = nullptr;
    QSlider* m_sliderZ = nullptr;
    QLabel*  m_labelX  = nullptr;
    QLabel*  m_labelY  = nullptr;
    QLabel*  m_labelZ  = nullptr;
    QPushButton* m_resetBtn = nullptr;
};
