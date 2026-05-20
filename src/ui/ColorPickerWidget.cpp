#include "ColorPickerWidget.h"

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <QSlider>
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
    if (m_color[0] == r && m_color[1] == g && m_color[2] == b) return;
    m_color = {r, g, b};
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
    m_preview->setStyleSheet(swatchStyle(m_color));
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

    m_rSlider->setValue(m_color[0]);
    m_gSlider->setValue(m_color[1]);
    m_bSlider->setValue(m_color[2]);
    m_rLabel->setText(QString::number(m_color[0]));
    m_gLabel->setText(QString::number(m_color[1]));
    m_bLabel->setText(QString::number(m_color[2]));
    m_hexEdit->setText(hexOf(m_color));
    m_preview->setStyleSheet(swatchStyle(m_color));

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
    emit colorChanged(m_color[0], m_color[1], m_color[2]);
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
