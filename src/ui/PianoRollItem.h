#pragma once

#include "core/PianoController.h"

#include <QPointer>
#include <QQuickPaintedItem>

class PianoRollItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(PianoController *controller READ controller WRITE setController NOTIFY controllerChanged)

public:
    explicit PianoRollItem(QQuickItem *parent = nullptr);

    PianoController *controller() const { return m_controller; }
    void setController(PianoController *controller);

    void paint(QPainter *painter) override;

signals:
    void controllerChanged();

private:
    double tickToBeat(qint64 tick) const;
    bool isBlackMidi(int midi) const;
    int whiteIndexBefore(int midi) const;
    qreal keyX(int midi, qreal whiteKeyWidth) const;
    QRectF noteRect(int midi, double startBeat, double durationBeat, double currentBeat,
                   qreal whiteKeyWidth, qreal fallTop, qreal strikeY, qreal pixelsPerBeat) const;
    void connectController(PianoController *controller);
    void disconnectController(PianoController *controller);

    QPointer<PianoController> m_controller;
};
