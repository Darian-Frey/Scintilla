#include "FrameInfoPanel.h"

#include <QLabel>
#include <QVBoxLayout>

FrameInfoPanel::FrameInfoPanel(QWidget* parent) : QWidget(parent) {
    buildLayout();
    refresh();
}

void FrameInfoPanel::buildLayout() {
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);

    m_gridLabel  = new QLabel(this);
    m_frameLabel = new QLabel(this);
    m_litLabel   = new QLabel(this);
    m_fpsLabel   = new QLabel(this);

    for (QLabel* l : {m_gridLabel, m_frameLabel, m_litLabel, m_fpsLabel}) {
        root->addWidget(l);
    }
    root->addStretch(1);
}

void FrameInfoPanel::setTimeline(AnimationTimeline* tl) {
    if (m_timeline) disconnect(m_timeline, nullptr, this, nullptr);
    m_timeline = tl;
    if (m_timeline) {
        connect(m_timeline, &AnimationTimeline::currentFrameChanged,    this, &FrameInfoPanel::refresh);
        connect(m_timeline, &AnimationTimeline::frameContentChanged,    this, &FrameInfoPanel::refresh);
        connect(m_timeline, &AnimationTimeline::timelineStructureChanged,this, &FrameInfoPanel::refresh);
    }
    refresh();
}

void FrameInfoPanel::setMask(std::shared_ptr<ShapeMask> mask) {
    m_mask = std::move(mask);
    refresh();
}

void FrameInfoPanel::refresh() {
    if (m_mask) {
        m_gridLabel->setText(
            tr("Shape: %1   Grid: %2")
                .arg(QString::fromStdString(shapeTypeName(m_mask->shape())))
                .arg(m_mask->gridSize())
            + QStringLiteral("³   ")
            + tr("LEDs: %1").arg(m_mask->count()));
    } else {
        m_gridLabel->setText(tr("Shape: —"));
    }
    if (m_timeline) {
        m_frameLabel->setText(tr("Frame: %1 / %2")
            .arg(m_timeline->currentIndex() + 1)
            .arg(m_timeline->frameCount()));
        m_litLabel->setText(tr("Lit voxels: %1").arg(m_timeline->currentFrame().litCount()));
        m_fpsLabel->setText(tr("FPS: %1").arg(m_timeline->fps()));
    } else {
        m_frameLabel->setText(tr("Frame: —"));
        m_litLabel->setText(tr("Lit voxels: —"));
        m_fpsLabel->setText(tr("FPS: —"));
    }
}
