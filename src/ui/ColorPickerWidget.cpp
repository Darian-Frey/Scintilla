#include "ColorPickerWidget.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSlider>
#include <QSpinBox>
#include <QVBoxLayout>
#include <algorithm>

namespace {

constexpr int kHistorySize = 16;
constexpr int kSwatchSize  = 22;

// 18 fixed palette colours (DEC: spec §3.6).
const std::array<RGB, 18> kPalette = {{
    {255,   0,   0}, {255, 128,   0}, {255, 255,   0},
    {128, 255,   0}, {  0, 255,   0}, {  0, 255, 128},
    {  0, 255, 255}, {  0, 128, 255}, {  0,   0, 255},
    { 96,   0, 255}, {192,   0, 255}, {255,   0, 192},
    {255, 128, 192}, {255, 255, 255}, {192, 192, 192},
    {128, 128, 128}, { 64,  64,  64}, {  0,   0,   0},
}};

QString hexOf(RGB c) {
    return QString("#%1%2%3")
        .arg(c[0], 2, 16, QChar('0'))
        .arg(c[1], 2, 16, QChar('0'))
        .arg(c[2], 2, 16, QChar('0'))
        .toUpper();
}

QString swatchStyle(RGB c) {
    return QString("background:%1; border:1px solid #222;").arg(hexOf(c));
}

// Scale a base RGB by a percentage brightness 0..100. The clamp is
// inherited from the integer division below, so no explicit guards needed.
RGB scaleBrightness(RGB c, int pct) {
    return {
        static_cast<uint8_t>(c[0] * pct / 100),
        static_cast<uint8_t>(c[1] * pct / 100),
        static_cast<uint8_t>(c[2] * pct / 100),
    };
}

}  // namespace

// ── Construction ─────────────────────────────────────────────────────────────

ColorPickerWidget::ColorPickerWidget(QWidget* parent) : QWidget(parent) {
    buildLayout();
    applyColorToUi();
}

void ColorPickerWidget::buildLayout() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    // Preview
    m_preview = new QLabel(this);
    m_preview->setFixedHeight(40);
    m_preview->setAlignment(Qt::AlignCenter);
    root->addWidget(m_preview);

    // Hex
    auto* hexRow = new QHBoxLayout();
    hexRow->addWidget(new QLabel(tr("Hex:"), this));
    m_hexEdit = new QLineEdit(this);
    m_hexEdit->setMaxLength(7);
    m_hexEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression(QStringLiteral("^#?[0-9A-Fa-f]{6}$")), this));
    connect(m_hexEdit, &QLineEdit::editingFinished, this, &ColorPickerWidget::onHexEdited);
    hexRow->addWidget(m_hexEdit);
    root->addLayout(hexRow);

    // Sliders
    auto sliderRow = [&](const QString& tag, QSlider*& s, QLabel*& l) {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel(tag, this));
        s = new QSlider(Qt::Horizontal, this);
        s->setRange(0, 255);
        connect(s, &QSlider::valueChanged, this, &ColorPickerWidget::onSliderChanged);
        row->addWidget(s, /*stretch*/ 1);
        l = new QLabel(this);
        l->setMinimumWidth(28);
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(l);
        root->addLayout(row);
    };
    sliderRow(tr("R"), m_rSlider, m_rLabel);
    sliderRow(tr("G"), m_gSlider, m_gLabel);
    sliderRow(tr("B"), m_bSlider, m_bLabel);

    // Brightness — multiplier applied to the base RGB before emit. The hex
    // and R/G/B sliders always show the BASE colour; the preview swatch
    // shows the dimmed result that's actually emitted to the viewport.
    // The QSpinBox alongside the slider is the precise-entry path — pixel
    // resolution on a short slider can otherwise skip individual values.
    {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel(tr("Brightness"), this));
        m_brightSlider = new QSlider(Qt::Horizontal, this);
        m_brightSlider->setRange(0, 100);
        m_brightSlider->setValue(100);
        m_brightSlider->setSingleStep(1);
        m_brightSlider->setPageStep(10);
        m_brightSlider->setToolTip(
            tr("Scales the paint colour. 100%% = full, 50%% = half, 0%% = off. "
               "Use the spinbox to type an exact value."));
        connect(m_brightSlider, &QSlider::valueChanged, this, [this](int v) {
            if (m_brightness == v) return;
            m_brightness = v;
            QSignalBlocker blk(m_brightSpin);
            m_brightSpin->setValue(v);
            applyColorToUi();
            emitChanged();
        });
        row->addWidget(m_brightSlider, 1);

        m_brightSpin = new QSpinBox(this);
        m_brightSpin->setRange(0, 100);
        m_brightSpin->setValue(100);
        m_brightSpin->setSuffix(QStringLiteral("%"));
        m_brightSpin->setMinimumWidth(60);
        m_brightSpin->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        m_brightSpin->setToolTip(tr("Type an exact brightness percentage 0–100."));
        connect(m_brightSpin, qOverload<int>(&QSpinBox::valueChanged), this, [this](int v) {
            if (m_brightness == v) return;
            m_brightness = v;
            QSignalBlocker blk(m_brightSlider);
            m_brightSlider->setValue(v);
            applyColorToUi();
            emitChanged();
        });
        row->addWidget(m_brightSpin);
        root->addLayout(row);
    }

    // Palette
    root->addWidget(new QLabel(tr("Palette"), this));
    auto* paletteGrid = new QGridLayout();
    paletteGrid->setSpacing(2);
    buildPalette(paletteGrid);
    root->addLayout(paletteGrid);

    // History
    root->addWidget(new QLabel(tr("Recent"), this));
    auto* historyGrid = new QGridLayout();
    historyGrid->setSpacing(2);
    buildHistory(historyGrid);
    root->addLayout(historyGrid);

    root->addStretch(1);
}

void ColorPickerWidget::buildPalette(QGridLayout* into) {
    m_paletteBtns.reserve(kPalette.size());
    for (size_t i = 0; i < kPalette.size(); ++i) {
        auto* b = new QPushButton(this);
        b->setFixedSize(kSwatchSize, kSwatchSize);
        b->setStyleSheet(swatchStyle(kPalette[i]));
        const int idx = static_cast<int>(i);
        connect(b, &QPushButton::clicked, this, [this, idx]() { onPaletteClicked(idx); });
        into->addWidget(b, idx / 9, idx % 9);
        m_paletteBtns.push_back(b);
    }
}

void ColorPickerWidget::buildHistory(QGridLayout* into) {
    m_historyBtns.reserve(kHistorySize);
    for (int i = 0; i < kHistorySize; ++i) {
        auto* b = new QPushButton(this);
        b->setFixedSize(kSwatchSize, kSwatchSize);
        b->setStyleSheet(QStringLiteral("background:#222; border:1px solid #444;"));
        connect(b, &QPushButton::clicked, this, [this, i]() { onHistoryClicked(i); });
        into->addWidget(b, i / 8, i % 8);
        m_historyBtns.push_back(b);
    }
}

// ── External API ─────────────────────────────────────────────────────────────

void ColorPickerWidget::setCurrentColor(uint8_t r, uint8_t g, uint8_t b) {
    if (m_color[0] == r && m_color[1] == g && m_color[2] == b && m_brightness == 100) return;
    // Picked colour is whatever's currently visible on the LED. Reset the
    // brightness slider so "what you sampled is what you paint" — without
    // this, picking a half-bright cell while the slider is at 50% would
    // emit a quarter-bright value on the next stroke.
    m_color = {r, g, b};
    m_brightness = 100;
    pushHistory(m_color);
    applyColorToUi();
    // No emitChanged() — pick-tool callers don't want a feedback loop.
}

// ── Slots ────────────────────────────────────────────────────────────────────

void ColorPickerWidget::onSliderChanged() {
    m_color = {
        static_cast<uint8_t>(m_rSlider->value()),
        static_cast<uint8_t>(m_gSlider->value()),
        static_cast<uint8_t>(m_bSlider->value()),
    };
    m_rLabel->setText(QString::number(m_color[0]));
    m_gLabel->setText(QString::number(m_color[1]));
    m_bLabel->setText(QString::number(m_color[2]));
    m_hexEdit->setText(hexOf(m_color));
    m_preview->setStyleSheet(swatchStyle(scaleBrightness(m_color, m_brightness)));
    emitChanged();
}

void ColorPickerWidget::onHexEdited() {
    QString s = m_hexEdit->text().trimmed();
    if (s.startsWith('#')) s.remove(0, 1);
    if (s.length() != 6) { applyColorToUi(); return; }
    bool ok = true;
    const uint8_t r = static_cast<uint8_t>(s.mid(0, 2).toInt(&ok, 16)); if (!ok) { applyColorToUi(); return; }
    const uint8_t g = static_cast<uint8_t>(s.mid(2, 2).toInt(&ok, 16)); if (!ok) { applyColorToUi(); return; }
    const uint8_t b = static_cast<uint8_t>(s.mid(4, 2).toInt(&ok, 16)); if (!ok) { applyColorToUi(); return; }
    m_color = {r, g, b};
    pushHistory(m_color);
    applyColorToUi();
    emitChanged();
}

void ColorPickerWidget::onPaletteClicked(int index) {
    if (index < 0 || static_cast<size_t>(index) >= kPalette.size()) return;
    m_color = kPalette[static_cast<size_t>(index)];
    pushHistory(m_color);
    applyColorToUi();
    emitChanged();
}

void ColorPickerWidget::onHistoryClicked(int slot) {
    if (slot < 0 || static_cast<size_t>(slot) >= m_history.size()) return;
    m_color = m_history[static_cast<size_t>(slot)];
    applyColorToUi();
    emitChanged();
}

// ── Internal helpers ─────────────────────────────────────────────────────────

void ColorPickerWidget::applyColorToUi() {
    // Block signals to avoid feedback during programmatic update.
    QSignalBlocker br(m_rSlider);
    QSignalBlocker bg(m_gSlider);
    QSignalBlocker bb(m_bSlider);
    QSignalBlocker bh(m_hexEdit);
    QSignalBlocker bx(m_brightSlider);

    m_rSlider->setValue(m_color[0]);
    m_gSlider->setValue(m_color[1]);
    m_bSlider->setValue(m_color[2]);
    m_brightSlider->setValue(m_brightness);
    m_rLabel->setText(QString::number(m_color[0]));
    m_gLabel->setText(QString::number(m_color[1]));
    m_bLabel->setText(QString::number(m_color[2]));
    { QSignalBlocker bsp(m_brightSpin); m_brightSpin->setValue(m_brightness); }
    m_hexEdit->setText(hexOf(m_color));
    m_preview->setStyleSheet(swatchStyle(scaleBrightness(m_color, m_brightness)));

    for (int i = 0; i < kHistorySize; ++i) {
        if (static_cast<size_t>(i) < m_history.size()) {
            m_historyBtns[static_cast<size_t>(i)]->setStyleSheet(
                swatchStyle(m_history[static_cast<size_t>(i)]));
            m_historyBtns[static_cast<size_t>(i)]->setEnabled(true);
        } else {
            m_historyBtns[static_cast<size_t>(i)]->setStyleSheet(
                QStringLiteral("background:#222; border:1px solid #444;"));
            m_historyBtns[static_cast<size_t>(i)]->setEnabled(false);
        }
    }
}

void ColorPickerWidget::emitChanged() {
    const RGB s = scaleBrightness(m_color, m_brightness);
    emit colorChanged(s[0], s[1], s[2]);
}

void ColorPickerWidget::pushHistory(RGB color) {
    // Dedupe: remove any existing instance, then prepend.
    m_history.erase(
        std::remove_if(m_history.begin(), m_history.end(),
                       [&](RGB c) { return c == color; }),
        m_history.end());
    m_history.insert(m_history.begin(), color);
    if (static_cast<int>(m_history.size()) > kHistorySize) {
        m_history.resize(kHistorySize);
    }
}
