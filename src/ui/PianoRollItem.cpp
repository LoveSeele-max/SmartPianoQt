#include "ui/PianoRollItem.h"

#include <QFont>
#include <QLinearGradient>
#include <QPainter>
#include <QRadialGradient>
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
    copy.setAlphaF(qBound<qreal>(0.0, alpha, 1.0));
    return copy;
}

qreal capsuleRadius(const QRectF &rect)
{
    return qMin(rect.width(), rect.height()) * 0.5;
}

QString beatLabel(int beat)
{
    return beat % 4 == 0 ? QStringLiteral("M%1").arg(beat / 4 + 1)
                         : QString::number(beat + 1);
}

} // namespace

PianoRollItem::PianoRollItem(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    setOpaquePainting(true);
    setRenderTarget(QQuickPaintedItem::FramebufferObject);
    setPerformanceHint(QQuickPaintedItem::FastFBOResizing, true);
    setFillColor(QColor("#070A12"));

    m_animationClock.start();
    m_feedbackClock.invalidate();
    m_animationTimer.setInterval(33);
    m_animationTimer.setTimerType(Qt::PreciseTimer);
    connect(&m_animationTimer, &QTimer::timeout, this, [this]() { update(); });
    m_animationTimer.start();
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

void PianoRollItem::setRollSpeedScale(qreal scale)
{
    const qreal normalized = qBound<qreal>(0.65, scale, 1.55);
    if (qFuzzyCompare(m_rollSpeedScale, normalized)) return;
    m_rollSpeedScale = normalized;
    emit rollSettingsChanged();
    update();
}

void PianoRollItem::setLookAheadBeats(qreal beats)
{
    const qreal normalized = qBound<qreal>(3.0, beats, 12.0);
    if (qFuzzyCompare(m_lookAheadBeats, normalized)) return;
    m_lookAheadBeats = normalized;
    emit rollSettingsChanged();
    update();
}

void PianoRollItem::setShowBeatRuler(bool visible)
{
    if (m_showBeatRuler == visible) return;
    m_showBeatRuler = visible;
    emit rollSettingsChanged();
    update();
}

void PianoRollItem::setSplitMidi(int midi)
{
    const int normalized = qBound(MinMidi, midi, MaxMidi);
    if (m_splitMidi == normalized) return;
    m_splitMidi = normalized;
    emit rollSettingsChanged();
    update();
}

void PianoRollItem::setHandDisplayMode(const QString &mode)
{
    const QString normalized = mode == QStringLiteral("dim") ? QStringLiteral("dim")
                                  : mode == QStringLiteral("all") ? QStringLiteral("all")
                                                                  : QStringLiteral("target");
    if (m_handDisplayMode == normalized) return;
    m_handDisplayMode = normalized;
    emit rollSettingsChanged();
    update();
}

void PianoRollItem::paint(QPainter *p)
{
    const QRectF bounds = boundingRect();
    const qreal w = bounds.width();
    const qreal h = bounds.height();
    if (w <= 1 || h <= 1) return;

    p->setRenderHint(QPainter::Antialiasing, true);

    QLinearGradient bg(0, 0, 0, h);
    bg.setColorAt(0.0, QColor("#111827"));
    bg.setColorAt(0.38, QColor("#0B1220"));
    bg.setColorAt(1.0, QColor("#05070D"));
    p->fillRect(bounds, bg);

    QLinearGradient sideShade(0, 0, w, 0);
    sideShade.setColorAt(0.0, QColor(13, 148, 136, 26));
    sideShade.setColorAt(0.18, QColor(13, 148, 136, 0));
    sideShade.setColorAt(0.82, QColor(26, 115, 232, 0));
    sideShade.setColorAt(1.0, QColor(26, 115, 232, 22));
    p->fillRect(bounds, sideShade);

    if (!m_controller) return;

    const qreal topPad = 34;
    const qreal bottomPad = 22;
    const qreal fallTop = topPad;
    const qreal strikeY = h - bottomPad;
    const qreal whiteKeyWidth = w / WhiteKeyCount;
    const qreal basePixelsPerBeat = (strikeY - fallTop) / qMax<qreal>(3.0, m_lookAheadBeats);
    const qreal pixelsPerBeat = qBound<qreal>(44, basePixelsPerBeat * m_rollSpeedScale, 170);
    const double currentBeat = tickToBeat(m_controller->currentTickValue());
    const qint64 visibleStartTick = qMax<qint64>(0, qRound64((currentBeat - 1.2) * m_controller->ppq()));
    const qint64 visibleEndTick = qRound64((currentBeat + m_lookAheadBeats + 1.0) * m_controller->ppq());
    const qint64 expectedTick = m_controller->expectedTickValue();
    const bool practiceMode = m_controller->mode() != QStringLiteral("auto");
    const bool handFilterActive = m_controller->handPracticeEnabled();
    const bool targetIsLeft = m_controller->handPracticeSide() == QStringLiteral("left");

    p->fillRect(QRectF(0, 0, w, topPad), QColor(5, 7, 13, 198));
    p->fillRect(QRectF(0, strikeY - 2, w, h - strikeY + 2), QColor(5, 7, 13, 218));

    for (int midi = MinMidi; midi <= MaxMidi; ++midi) {
        if (isBlackMidi(midi)) continue;
        const qreal x = keyX(midi, whiteKeyWidth);
        const QRectF lane(x, topPad, whiteKeyWidth, strikeY - topPad);
        const int pc = ((midi % 12) + 12) % 12;
        p->fillRect(lane, whiteIndexBefore(midi) % 2 == 0 ? QColor(255, 255, 255, 5)
                                                          : QColor(255, 255, 255, 2));
        if (pc == 0 || pc == 5) {
            p->setPen(QPen(QColor(255, 255, 255, 16), 0.7));
            p->drawLine(QPointF(x, topPad), QPointF(x, strikeY));
        }
    }

    for (int midi = MinMidi; midi <= MaxMidi; ++midi) {
        if (!isBlackMidi(midi)) continue;
        const qreal x = keyX(midi, whiteKeyWidth);
        p->fillRect(QRectF(x, topPad, whiteKeyWidth * 0.64, strikeY - topPad),
                    QColor(0, 0, 0, 20));
    }

    if (m_controller->loopRangeValid()) {
        const qreal loopStartY = strikeY - (m_controller->loopStartBeat() - currentBeat) * pixelsPerBeat;
        const qreal loopEndY = strikeY - (m_controller->loopEndBeat() - currentBeat) * pixelsPerBeat;
        const qreal top = qMax<qreal>(topPad, qMin(loopStartY, loopEndY));
        const qreal bottom = qMin<qreal>(strikeY, qMax(loopStartY, loopEndY));
        if (bottom > top) {
            p->fillRect(QRectF(0, top, w, bottom - top), QColor(126, 87, 194, 34));
        }
        p->setPen(QPen(QColor(196, 181, 253, 150), 1.2));
        if (loopStartY >= topPad && loopStartY <= strikeY) p->drawLine(QPointF(0, loopStartY), QPointF(w, loopStartY));
        if (loopEndY >= topPad && loopEndY <= strikeY) p->drawLine(QPointF(0, loopEndY), QPointF(w, loopEndY));
    }

    if (m_showBeatRuler) {
        const int startBeat = qMax(0, int(std::floor(currentBeat - 1)));
        const int endBeat = int(std::ceil(qMin(tickToBeat(m_controller->totalTickValue()) + 1.0,
                                             currentBeat + m_lookAheadBeats + 1.0)));
        for (int beat = startBeat; beat <= endBeat; ++beat) {
            const qreal y = strikeY - (beat - currentBeat) * pixelsPerBeat;
            if (y < topPad || y > strikeY) continue;
            const bool measure = beat % 4 == 0;
            QPen pen(measure ? QColor(125, 211, 252, 82) : QColor(255, 255, 255, 16));
            pen.setWidthF(measure ? 1.1 : 0.45);
            p->setPen(pen);
            p->drawLine(QPointF(0, y), QPointF(w, y));

            const QString label = beatLabel(beat);
            const QRectF chip(10, y - 12, measure ? 48 : 34, 24);
            p->setPen(Qt::NoPen);
            p->setBrush(measure ? QColor(26, 115, 232, 132) : QColor(255, 255, 255, 30));
            p->drawRoundedRect(chip, 12, 12);
            p->setPen(measure ? QColor("#D3E3FD") : QColor(203, 213, 225, 170));
            QFont font(QStringLiteral("Segoe UI"), measure ? 8 : 7);
            font.setBold(measure);
            p->setFont(font);
            p->drawText(chip, Qt::AlignCenter, label);
        }
    }

    QLinearGradient strikeGlow(0, strikeY - 18, 0, strikeY + 6);
    strikeGlow.setColorAt(0.0, QColor(0, 163, 137, 0));
    strikeGlow.setColorAt(0.58, QColor(0, 163, 137, 16));
    strikeGlow.setColorAt(1.0, QColor(0, 163, 137, 0));
    p->fillRect(QRectF(0, strikeY - 18, w, 24), strikeGlow);
    p->setPen(QPen(QColor(0, 163, 137, 18), 2.4, Qt::SolidLine, Qt::RoundCap));
    p->drawLine(QPointF(0, strikeY), QPointF(w, strikeY));
    p->setPen(QPen(QColor(94, 234, 212, 64), 1.4, Qt::SolidLine, Qt::RoundCap));
    p->drawLine(QPointF(0, strikeY), QPointF(w, strikeY));
    p->setPen(QPen(QColor(190, 244, 232, 136), 1.0, Qt::SolidLine, Qt::RoundCap));
    p->drawLine(QPointF(0, strikeY), QPointF(w, strikeY));

    p->setPen(QColor(226, 232, 240, 210));
    QFont titleFont(QStringLiteral("Segoe UI"), 9);
    titleFont.setBold(true);
    p->setFont(titleFont);
    p->drawText(QRectF(14, 0, w - 28, topPad), Qt::AlignVCenter | Qt::AlignLeft,
                QStringLiteral("瀑布视图 · 音符落到判定线时演奏"));

    const auto &notes = m_controller->noteEvents();
    const qint64 searchStartTick = qMax<qint64>(0, visibleStartTick - qint64(m_controller->ppq()) * 8);
    const auto first = std::lower_bound(notes.begin(), notes.end(), searchStartTick,
        [](const NoteEvent &note, qint64 tick) {
            return note.startTick < tick;
        });

    const qint64 currentTick = m_controller->currentTickValue();
    const qreal pulse = 0.5 + 0.5 * std::sin(double(m_animationClock.elapsed()) / 260.0);
    auto drawNotes = [&](bool preparatoryPass) {
        for (auto it = first; it != notes.end(); ++it) {
            const NoteEvent &note = *it;
            if (note.startTick > visibleEndTick) break;

            const double startBeat = tickToBeat(note.startTick);
            const double durationBeat = tickToBeat(note.durationTick);
            const QRectF rect = noteRect(note.midi, startBeat, durationBeat, currentBeat,
                                         whiteKeyWidth, fallTop, strikeY, pixelsPerBeat);
            if (rect.bottom() < topPad || rect.top() > strikeY + 36) continue;

            const bool noteIsLeft = note.midi < m_splitMidi;
            const bool targetHand = !handFilterActive || (targetIsLeft ? noteIsLeft : !noteIsLeft);
            const bool active = currentTick >= note.startTick &&
                                currentTick <= note.startTick + note.durationTick;
            const bool expected = practiceMode && expectedTick == note.startTick && targetHand;
            const bool preparatory = !expected && !active && !note.played && note.startTick > currentTick;
            if (preparatory != preparatoryPass) continue;

            if (handFilterActive && m_handDisplayMode == QStringLiteral("target") && !targetHand) {
                continue;
            }

            QColor fill = handColor(note.midi);
            QColor edge = handEdgeColor(note.midi);
            qreal fillAlpha = 0.86;
            qreal edgeAlpha = 0.86;
            if (note.played) {
                fill = QColor("#64748B");
                edge = QColor("#94A3B8");
                fillAlpha = 0.56;
                edgeAlpha = 0.48;
            }
            if (active) {
                fill = QColor("#5EEAD4");
                edge = QColor("#CCFBF1");
                fillAlpha = 0.94;
                edgeAlpha = 0.96;
            }
            if (expected) {
                fill = QColor("#F9AB00");
                edge = QColor("#FEEFC3");
                fillAlpha = 0.88 + pulse * 0.12;
                edgeAlpha = 0.92;
                const QRectF glowRect = rect.adjusted(-4 - pulse * 5, -4 - pulse * 5,
                                                      4 + pulse * 5, 4 + pulse * 5);
                p->setPen(Qt::NoPen);
                p->setBrush(withAlpha(QColor("#FEEFC3"), 0.16 + pulse * 0.18));
                p->drawRoundedRect(glowRect, capsuleRadius(glowRect), capsuleRadius(glowRect));
            }
            if (preparatory) {
                fillAlpha = 0.16;
                edgeAlpha = 0.42;
            }
            if (handFilterActive && m_handDisplayMode == QStringLiteral("dim") && !targetHand) {
                fillAlpha *= 0.34;
                edgeAlpha *= 0.42;
            }

            QLinearGradient noteGradient(rect.topLeft(), rect.bottomRight());
            noteGradient.setColorAt(0.0, withAlpha(fill.lighter(122), fillAlpha));
            noteGradient.setColorAt(1.0, withAlpha(fill.darker(112), fillAlpha));
            p->setPen(Qt::NoPen);
            p->setBrush(noteGradient);
            p->drawRoundedRect(rect, capsuleRadius(rect), capsuleRadius(rect));
            p->setBrush(Qt::NoBrush);
            p->setPen(QPen(withAlpha(edge, edgeAlpha), expected ? 1.8 : 1.1));
            p->drawRoundedRect(rect.adjusted(0.5, 0.5, -0.5, -0.5),
                               capsuleRadius(rect), capsuleRadius(rect));
        }
    };

    drawNotes(true);
    drawNotes(false);
    drawFeedback(p, bounds, strikeY);
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
    const qreal width = black ? whiteKeyWidth * 0.58 : whiteKeyWidth * 0.82;
    const qreal x = keyX(midi, whiteKeyWidth) + (black ? whiteKeyWidth * 0.03 : whiteKeyWidth * 0.09);
    const qreal endY = strikeY - (startBeat - currentBeat) * pixelsPerBeat;
    const qreal height = qMax<qreal>(durationBeat * pixelsPerBeat, 18.0);
    const qreal top = qMax<qreal>(fallTop - 48, endY - height);
    return QRectF(x, top, width, height);
}

QColor PianoRollItem::handColor(int midi) const
{
    return midi < m_splitMidi ? QColor("#A78BFA") : QColor("#38BDF8");
}

QColor PianoRollItem::handEdgeColor(int midi) const
{
    return midi < m_splitMidi ? QColor("#DDD6FE") : QColor("#BAE6FD");
}

QColor PianoRollItem::feedbackColor(FeedbackKind kind) const
{
    switch (kind) {
    case FeedbackKind::Perfect:
        return QColor("#22C55E");
    case FeedbackKind::Good:
        return QColor("#60A5FA");
    case FeedbackKind::Early:
    case FeedbackKind::Late:
        return QColor("#F9AB00");
    case FeedbackKind::Wrong:
    case FeedbackKind::Missed:
        return QColor("#EF4444");
    case FeedbackKind::None:
        break;
    }
    return QColor("#E2E8F0");
}

void PianoRollItem::drawFeedback(QPainter *p, const QRectF &bounds, qreal strikeY)
{
    if (!m_feedbackClock.isValid() || m_feedbackText.isEmpty()) return;
    const qreal elapsed = qreal(m_feedbackClock.elapsed());
    const qreal duration = 920.0;
    if (elapsed >= duration) return;

    const qreal progress = elapsed / duration;
    const QColor base = feedbackColor(m_feedbackKind);
    if ((m_feedbackKind == FeedbackKind::Wrong || m_feedbackKind == FeedbackKind::Missed) && elapsed < 260.0) {
        const qreal flash = 1.0 - elapsed / 260.0;
        QLinearGradient redFlash(0, strikeY - 70, 0, strikeY + 16);
        redFlash.setColorAt(0.0, QColor(239, 68, 68, 0));
        redFlash.setColorAt(0.58, withAlpha(base, 0.30 * flash));
        redFlash.setColorAt(1.0, QColor(239, 68, 68, 0));
        p->fillRect(QRectF(0, strikeY - 70, bounds.width(), 88), redFlash);
    }

    const qreal alpha = 1.0 - progress;
    const QRectF textRect(bounds.width() * 0.5 - 120, strikeY - 96 - progress * 34, 240, 44);
    QFont font(QStringLiteral("Segoe UI"), 22);
    font.setBold(true);
    p->setFont(font);
    p->setPen(withAlpha(QColor("#020617"), 0.26 * alpha));
    p->drawText(textRect.translated(0, 2), Qt::AlignCenter, m_feedbackText);
    p->setPen(withAlpha(base, alpha));
    p->drawText(textRect, Qt::AlignCenter, m_feedbackText);
}

void PianoRollItem::triggerFeedbackFromStatus()
{
    if (!m_controller) return;

    const QString status = m_controller->statusMessage();
    FeedbackKind kind = FeedbackKind::None;
    QString text;
    if (status.contains(QStringLiteral("Perfect"))) {
        kind = FeedbackKind::Perfect;
        text = QStringLiteral("Perfect");
    } else if (status.contains(QStringLiteral("Good"))) {
        kind = FeedbackKind::Good;
        text = QStringLiteral("Good");
    } else if (status.contains(QStringLiteral("偏早"))) {
        kind = FeedbackKind::Early;
        text = QStringLiteral("Early");
    } else if (status.contains(QStringLiteral("偏晚"))) {
        kind = FeedbackKind::Late;
        text = QStringLiteral("Late");
    } else if (status.contains(QStringLiteral("错音"))) {
        kind = FeedbackKind::Wrong;
        text = QStringLiteral("Wrong");
    } else if (status.contains(QStringLiteral("漏弹"))) {
        kind = FeedbackKind::Missed;
        text = QStringLiteral("Missed");
    }

    if (kind == FeedbackKind::None) return;
    m_feedbackKind = kind;
    m_feedbackText = text;
    m_feedbackClock.restart();
    update();
}

void PianoRollItem::connectController(PianoController *controller)
{
    connect(controller, &PianoController::frameChanged, this, [this]() { update(); });
    connect(controller, &PianoController::positionChanged, this, [this]() { update(); });
    connect(controller, &PianoController::notesChanged, this, [this]() { update(); });
    connect(controller, &PianoController::practiceChanged, this, [this]() { update(); });
    connect(controller, &PianoController::modeChanged, this, [this]() { update(); });
    connect(controller, &PianoController::songChanged, this, [this]() { update(); });
    connect(controller, &PianoController::loopPracticeChanged, this, [this]() { update(); });
    connect(controller, &PianoController::handPracticeChanged, this, [this]() { update(); });
    connect(controller, &PianoController::statusMessageChanged, this, [this]() {
        triggerFeedbackFromStatus();
    });
}

void PianoRollItem::disconnectController(PianoController *controller)
{
    disconnect(controller, nullptr, this, nullptr);
}
