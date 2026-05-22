#include "ui/PianoRollItem.h"

#include "core/NoteUtils.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QtMath>
#include <algorithm>
#include <cmath>

namespace {

constexpr int MinMidi = 36;
constexpr int MaxMidi = 96;

QColor withAlpha(const QColor &color, qreal alpha)
{
    QColor copy = color;
    copy.setAlphaF(alpha);
    return copy;
}

}

PianoRollItem::PianoRollItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(true);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setPerformanceHint(QQuickPaintedItem::FastFBOResizing, true);
    setFillColor(QColor("#09090b"));
}

void PianoRollItem::setController(PianoController *controller)
{
    if (m_controller == controller) return;

    if (m_controller) disconnectController(m_controller);
    m_controller = controller;
    if (m_controller) connectController(m_controller);

    emit controllerChanged();
    update();
}

void PianoRollItem::paint(QPainter *p)
{
    const QRectF bounds = boundingRect();
    const qreal w = bounds.width();
    const qreal h = bounds.height();
    if (w <= 1 || h <= 1) return;

    p->setRenderHint(QPainter::Antialiasing, true);
    p->fillRect(bounds, QColor("#09090b"));

    QLinearGradient bg(0, 0, w, h);
    bg.setColorAt(0.0, QColor(18, 161, 177, 42));
    bg.setColorAt(0.45, QColor(24, 24, 27, 210));
    bg.setColorAt(1.0, QColor(12, 14, 18, 255));
    p->fillRect(bounds, bg);

    if (!m_controller) return;

    const qreal gutter = 58;
    const qreal ruler = 34;
    const qreal bottomPad = 24;
    const qreal trackTop = ruler + 12;
    const qreal trackBottom = h - bottomPad;
    const qreal trackHeight = qMax<qreal>(120, trackBottom - trackTop);
    const qreal playheadX = gutter + (w - gutter) * 0.42;
    const qreal pixelsPerBeat = qBound<qreal>(58, (w - gutter) / 11.0, 120);
    const double currentBeat = tickToBeat(m_controller->currentTickValue());
    const qint64 visibleStartTick = qMax<qint64>(0, qRound64((currentBeat - 6.0) * m_controller->ppq()));
    const qint64 visibleEndTick = qRound64((currentBeat + 9.0) * m_controller->ppq());
    const qint64 expectedTick = m_controller->expectedTickValue();
    const bool practiceMode = m_controller->mode() == QStringLiteral("practice");

    p->fillRect(QRectF(0, 0, gutter, h), QColor(24, 24, 27, 235));
    p->fillRect(QRectF(gutter, 0, w - gutter, ruler), QColor(9, 9, 11, 200));

    QPen gridPen;
    for (int midi = MinMidi; midi <= MaxMidi; ++midi) {
        const qreal y = noteY(midi, trackTop, trackHeight);
        const bool octave = midi % 12 == 0;
        gridPen.setColor(octave ? QColor(20, 184, 166, 56) : QColor(255, 255, 255, 12));
        gridPen.setWidthF(octave ? 1.0 : 0.5);
        p->setPen(gridPen);
        p->drawLine(QPointF(gutter, y), QPointF(w, y));

        if (octave) {
            p->setPen(QColor(228, 228, 231, 184));
            QFont font(QStringLiteral("Segoe UI"), 8);
            p->setFont(font);
            p->drawText(QRectF(2, y - 8, gutter - 11, 16), Qt::AlignRight | Qt::AlignVCenter,
                        NoteUtils::midiToName(midi));
        }
    }

    const int startBeat = qMax(0, int(std::floor(currentBeat - 5)));
    const int endBeat = int(std::ceil(qMin(tickToBeat(m_controller->totalTickValue()) + 1.0, currentBeat + 8.0)));
    for (int beat = startBeat; beat <= endBeat; ++beat) {
        const qreal x = (beat - currentBeat) * pixelsPerBeat + playheadX;
        const bool measure = beat % 4 == 0;
        QPen pen(measure ? QColor(20, 184, 166, 90) : QColor(255, 255, 255, 20));
        pen.setWidthF(measure ? 1.2 : 0.5);
        p->setPen(pen);
        p->drawLine(QPointF(x, ruler), QPointF(x, h));

        p->setPen(measure ? QColor(153, 246, 228, 240) : QColor(161, 161, 170, 148));
        QFont font(QStringLiteral("Segoe UI"), measure ? 8 : 7);
        font.setBold(measure);
        p->setFont(font);
        p->drawText(QRectF(x - 28, 0, 56, ruler), Qt::AlignCenter,
                    measure ? QStringLiteral("M%1").arg(beat / 4 + 1) : QString::number(beat + 1));
    }

    const auto &notes = m_controller->noteEvents();
    const qint64 searchStartTick = qMax<qint64>(0, visibleStartTick - qint64(m_controller->ppq()) * 8);
    const auto first = std::lower_bound(notes.begin(), notes.end(), searchStartTick,
        [](const NoteEvent &note, qint64 tick) {
            return note.startTick < tick;
        });

    QFont noteFont(QStringLiteral("Segoe UI"), 8);
    noteFont.setBold(true);

    for (auto it = first; it != notes.end(); ++it) {
        const NoteEvent &note = *it;
        if (note.startTick > visibleEndTick) break;

        const double startBeat = tickToBeat(note.startTick);
        const double durationBeat = tickToBeat(note.durationTick);
        const qreal x = (startBeat - currentBeat) * pixelsPerBeat + playheadX;
        const qreal noteW = qMax<qreal>(durationBeat * pixelsPerBeat - 10, 20);
        if (x < gutter - noteW - 70 || x > w + 70) continue;

        const qreal y = noteY(note.midi, trackTop, trackHeight);
        const bool active = m_controller->currentTickValue() >= note.startTick &&
                            m_controller->currentTickValue() <= note.startTick + note.durationTick;
        const bool expected = practiceMode && expectedTick == note.startTick;
        const QColor fill = expected ? QColor("#facc15")
                          : active ? QColor("#5eead4")
                          : note.played ? QColor("#64748b")
                          : QColor("#38bdf8");
        const QColor edge = expected ? QColor("#fef08a")
                          : active ? QColor("#ccfbf1")
                          : QColor("#bae6fd");

        const QRectF rect(x, y - 9, noteW, 18);
        p->setPen(Qt::NoPen);
        p->setBrush(withAlpha(fill, expected || active ? 0.96 : 0.86));
        p->drawRoundedRect(rect, 5, 5);
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(edge, 1.0));
        p->drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), 5, 5);

        if (noteW > 34) {
            p->setPen(QColor("#020617"));
            p->setFont(noteFont);
            p->drawText(rect, Qt::AlignCenter, note.noteName);
        }
    }

    QPen playhead(QColor("#f8fafc"), 2.0);
    p->setPen(playhead);
    p->drawLine(QPointF(playheadX, ruler), QPointF(playheadX, h));
}

double PianoRollItem::tickToBeat(qint64 tick) const
{
    if (!m_controller || m_controller->ppq() <= 0) return 0.0;
    return double(tick) / double(m_controller->ppq());
}

qreal PianoRollItem::noteY(int midi, qreal trackTop, qreal trackHeight) const
{
    const qreal span = MaxMidi - MinMidi;
    return trackTop + (1.0 - (midi - MinMidi) / span) * trackHeight;
}

void PianoRollItem::connectController(PianoController *controller)
{
    connect(controller, &PianoController::frameChanged, this, [this]() { update(); });
    connect(controller, &PianoController::positionChanged, this, [this]() { update(); });
    connect(controller, &PianoController::notesChanged, this, [this]() { update(); });
    connect(controller, &PianoController::practiceChanged, this, [this]() { update(); });
    connect(controller, &PianoController::modeChanged, this, [this]() { update(); });
    connect(controller, &PianoController::songChanged, this, [this]() { update(); });
}

void PianoRollItem::disconnectController(PianoController *controller)
{
    disconnect(controller, nullptr, this, nullptr);
}
