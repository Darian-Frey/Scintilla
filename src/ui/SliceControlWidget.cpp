#include "SliceControlWidget.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>
#include <algorithm>

namespace {
    QString sliceText(int v) {
        return (v < 0) ? QStringLiteral("all") : QString::number(v);
    }
}

SliceControlWidget::SliceControlWidget(QWidget* parent) : QWidget(parent) {
    buildLayout();
    refreshLabels();
}

void SliceControlWidget::buildLayout() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    auto makeRow = [&](const QString& tag, QSlider*& s, QLabel*& l) {
        auto* row = new QHBoxLayout();
        row->addWidget(new QLabel(tag, this));
        s = new QSlider(Qt::Horizontal, this);
        s->setRange(-1, m_gridSize - 1);
        s->setValue(-1);
        connect(s, &QSlider::valueChanged, this, &SliceControlWidget::onSliderChanged);
        row->addWidget(s, /*stretch*/ 1);
        l = new QLabel(this);
        l->setMinimumWidth(28);
        l->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        row->addWidget(l);
        root->addLayout(row);
    };
    makeRow(tr("X"), m_sliderX, m_labelX);
    makeRow(tr("Y"), m_sliderY, m_labelY);
    makeRow(tr("Z"), m_sliderZ, m_labelZ);

    m_resetBtn = new QPushButton(tr("Show all layers"), this);
    connect(m_resetBtn, &QPushButton::clicked, this, &SliceControlWidget::resetAll);
    root->addWidget(m_resetBtn);

    root->addStretch(1);
}

void SliceControlWidget::setGridSize(int n) {
    m_gridSize = std::max(3, n);
    for (auto* s : {m_sliderX, m_sliderY, m_sliderZ}) {
        QSignalBlocker block(s);
        s->setRange(-1, m_gridSize - 1);
        if (s->value() > m_gridSize - 1) s->setValue(m_gridSize - 1);
    }
    refreshLabels();
    emit sliceChanged(sliceX(), sliceY(), sliceZ());
}

int SliceControlWidget::sliceX() const { return m_sliderX->value(); }
int SliceControlWidget::sliceY() const { return m_sliderY->value(); }
int SliceControlWidget::sliceZ() const { return m_sliderZ->value(); }

void SliceControlWidget::resetAll() {
    for (auto* s : {m_sliderX, m_sliderY, m_sliderZ}) {
        QSignalBlocker block(s);
        s->setValue(-1);
    }
    refreshLabels();
    emit sliceChanged(-1, -1, -1);
}

void SliceControlWidget::onSliderChanged() {
    refreshLabels();
    emit sliceChanged(sliceX(), sliceY(), sliceZ());
}

void SliceControlWidget::refreshLabels() {
    m_labelX->setText(sliceText(sliceX()));
    m_labelY->setText(sliceText(sliceY()));
    m_labelZ->setText(sliceText(sliceZ()));
}
