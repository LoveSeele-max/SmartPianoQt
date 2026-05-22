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
    qreal noteY(int midi, qreal trackTop, qreal trackHeight) const;
    void connectController(PianoController *controller);
    void disconnectController(PianoController *controller);

    QPointer<PianoController> m_controller;
};
