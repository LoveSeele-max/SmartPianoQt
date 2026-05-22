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
constexpr int WhiteKeyCount = 36;

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

    const qreal topPad = 32;
    const qreal bottomPad = 20;
    const qreal fallTop = topPad;
    const qreal strikeY = h - bottomPad;
    const qreal whiteKeyWidth = w / WhiteKeyCount;
    const qreal pixelsPerBeat = qBound<qreal>(58, (strikeY - fallTop) / 6.0, 130);
    const double currentBeat = tickToBeat(m_controller->currentTickValue());
    const qint64 visibleStartTick = qMax<qint64>(0, qRound64((currentBeat - 1.0) * m_controller->ppq()));
    const qint64 visibleEndTick = qRound64((currentBeat + 7.0) * m_controller->ppq());
    const qint64 expectedTick = m_controller->expectedTickValue();
    const bool practiceMode = m_controller->mode() == QStringLiteral("practice");

    p->fillRect(QRectF(0, 0, w, topPad), QColor(9, 9, 11, 210));
    p->fillRect(QRectF(0, strikeY - 2, w, h - strikeY + 2), QColor(9, 9, 11, 225));

    for (int midi = MinMidi; midi <= MaxMidi; ++midi) {
        if (isBlackMidi(midi)) continue;
        const qreal x = keyX(midi, whiteKeyWidth);
        const QRectF lane(x, topPad, whiteKeyWidth, strikeY - topPad);
        p->fillRect(lane, whiteIndexBefore(midi) % 2 == 0 ? QColor(255, 255, 255, 10)
                                                          : QColor(255, 255, 255, 6));
        p->setPen(QPen(QColor(255, 255, 255, 18), 0.6));
        p->drawLine(QPointF(x, topPad), QPointF(x, strikeY));
    }

    for (int midi = MinMidi; midi <= MaxMidi; ++midi) {
        if (!isBlackMidi(midi)) continue;
        const qreal x = keyX(midi, whiteKeyWidth);
        p->fillRect(QRectF(x, topPad, whiteKeyWidth * 0.64, strikeY - topPad),
                    QColor(0, 0, 0, 34));
    }

    const int startBeat = qMax(0, int(std::floor(currentBeat - 1)));
    const int endBeat = int(std::ceil(qMin(tickToBeat(m_controller->totalTickValue()) + 1.0, currentBeat + 7.0)));
    for (int beat = startBeat; beat <= endBeat; ++beat) {
        const qreal y = strikeY - (beat - currentBeat) * pixelsPerBeat;
        if (y < topPad || y > strikeY) continue;
        const bool measure = beat % 4 == 0;
        QPen pen(measure ? QColor(20, 184, 166, 90) : QColor(255, 255, 255, 20));
        pen.setWidthF(measure ? 1.2 : 0.5);
        p->setPen(pen);
        p->drawLine(QPointF(0, y), QPointF(w, y));

        p->setPen(measure ? QColor(153, 246, 228, 240) : QColor(161, 161, 170, 148));
        QFont font(QStringLiteral("Segoe UI"), measure ? 8 : 7);
        font.setBold(measure);
        p->setFont(font);
        p->drawText(QRectF(8, y - 11, 58, 22), Qt::AlignLeft | Qt::AlignVCenter,
                    measure ? QStringLiteral("M%1").arg(beat / 4 + 1) : QString::number(beat + 1));
    }

    QLinearGradient strikeGlow(0, strikeY - 28, 0, strikeY + 8);
    strikeGlow.setColorAt(0.0, QColor(20, 184, 166, 0));
    strikeGlow.setColorAt(0.62, QColor(20, 184, 166, 58));
    strikeGlow.setColorAt(1.0, QColor(20, 184, 166, 0));
    p->fillRect(QRectF(0, strikeY - 28, w, 36), strikeGlow);
    p->setPen(QPen(QColor("#f8fafc"), 2.0));
    p->drawLine(QPointF(0, strikeY), QPointF(w, strikeY));

    p->setPen(QColor(228, 228, 231, 185));
    QFont titleFont(QStringLiteral("Segoe UI"), 9);
    titleFont.setBold(true);
    p->setFont(titleFont);
    p->drawText(QRectF(12, 0, w - 24, topPad), Qt::AlignVCenter | Qt::AlignLeft,
                QStringLiteral("瀑布视图：音符落到键盘上沿时弹奏"));

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
        const QRectF rect = noteRect(note.midi, startBeat, durationBeat, currentBeat,
                                     whiteKeyWidth, fallTop, strikeY, pixelsPerBeat);
        if (rect.bottom() < topPad || rect.top() > strikeY + 30) continue;

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

        p->setPen(Qt::NoPen);
        p->setBrush(withAlpha(fill, expected || active ? 0.96 : 0.86));
        p->drawRoundedRect(rect, 6, 6);
        p->setBrush(Qt::NoBrush);
        p->setPen(QPen(edge, expected ? 1.5 : 1.0));
        p->drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5), 6, 6);

        if (rect.height() > 18 && rect.width() > 22) {
            p->setPen(QColor("#020617"));
            p->setFont(noteFont);
            p->drawText(rect, Qt::AlignCenter, note.noteName);
        }
    }
}

double PianoRollItem::tickToBeat(qint64 tick) const
{
    if (!m_controller || m_controller->ppq() <= 0) return 0.0;
    return double(tick) / double(m_controller->ppq());
}

bool PianoRollItem::isBlackMidi(int midi) const
{
    const int pc = ((midi % 12) + 12) % 12;
    return pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
}

int PianoRollItem::whiteIndexBefore(int midi) const
{
    int count = 0;
    for (int n = MinMidi; n < midi; ++n) {
        if (!isBlackMidi(n)) ++count;
    }
    return count;
}

qreal PianoRollItem::keyX(int midi, qreal whiteKeyWidth) const
{
    if (!isBlackMidi(midi)) {
        return whiteIndexBefore(midi) * whiteKeyWidth;
    }
    return whiteIndexBefore(midi) * whiteKeyWidth - whiteKeyWidth * 0.32;
}

QRectF PianoRollItem::noteRect(int midi, double startBeat, double durationBeat, double currentBeat,
                               qreal whiteKeyWidth, qreal fallTop, qreal strikeY, qreal pixelsPerBeat) const
{
    const bool black = isBlackMidi(midi);
    const qreal width = black ? whiteKeyWidth * 0.64 : whiteKeyWidth * 0.88;
    const qreal x = keyX(midi, whiteKeyWidth) + (black ? 0.0 : whiteKeyWidth * 0.06);
    const qreal endY = strikeY - (startBeat - currentBeat) * pixelsPerBeat;
    const qreal height = qMax<qreal>(durationBeat * pixelsPerBeat, 18.0);
    const qreal top = qMax<qreal>(fallTop - 42, endY - height);
    return QRectF(x, top, width, height);
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
