#pragma once

#include "core/PianoController.h"

#include <QElapsedTimer>
#include <QPointer>
#include <QQuickPaintedItem>
#include <QTimer>

class PianoRollItem : public QQuickPaintedItem {
    Q_OBJECT
    Q_PROPERTY(PianoController *controller READ controller WRITE setController NOTIFY controllerChanged)
    Q_PROPERTY(qreal rollSpeedScale READ rollSpeedScale WRITE setRollSpeedScale NOTIFY rollSettingsChanged)
    Q_PROPERTY(qreal lookAheadBeats READ lookAheadBeats WRITE setLookAheadBeats NOTIFY rollSettingsChanged)
    Q_PROPERTY(bool showBeatRuler READ showBeatRuler WRITE setShowBeatRuler NOTIFY rollSettingsChanged)
    Q_PROPERTY(QString handDisplayMode READ handDisplayMode WRITE setHandDisplayMode NOTIFY rollSettingsChanged)

public:
    explicit PianoRollItem(QQuickItem *parent = nullptr);

    PianoController *controller() const { return m_controller; }
    void setController(PianoController *controller);
    qreal rollSpeedScale() const { return m_rollSpeedScale; }
    void setRollSpeedScale(qreal scale);
    qreal lookAheadBeats() const { return m_lookAheadBeats; }
    void setLookAheadBeats(qreal beats);
    bool showBeatRuler() const { return m_showBeatRuler; }
    void setShowBeatRuler(bool visible);
    QString handDisplayMode() const { return m_handDisplayMode; }
    void setHandDisplayMode(const QString &mode);

    void paint(QPainter *painter) override;

signals:
    void controllerChanged();
    void rollSettingsChanged();

private:
    enum class FeedbackKind {
        None,
        Perfect,
        Good,
        Early,
        Late,
        Wrong,
        Missed
    };

    double tickToBeat(qint64 tick) const;
    bool isBlackMidi(int midi) const;
    int whiteIndexBefore(int midi) const;
    qreal keyX(int midi, qreal whiteKeyWidth) const;
    QRectF noteRect(int midi, double startBeat, double durationBeat, double currentBeat,
                   qreal whiteKeyWidth, qreal fallTop, qreal strikeY, qreal pixelsPerBeat) const;
    QColor handColor(const NoteEvent &note) const;
    QColor handEdgeColor(const NoteEvent &note) const;
    QColor feedbackColor(FeedbackKind kind) const;
    void drawFeedback(QPainter *painter, const QRectF &bounds, qreal strikeY);
    void triggerFeedbackFromStatus();
    void connectController(PianoController *controller);
    void disconnectController(PianoController *controller);

    QPointer<PianoController> m_controller;
    QElapsedTimer m_animationClock;
    QElapsedTimer m_feedbackClock;
    QTimer m_animationTimer;
    qreal m_rollSpeedScale = 1.0;
    qreal m_lookAheadBeats = 7.0;
    bool m_showBeatRuler = true;
    QString m_handDisplayMode = QStringLiteral("target");
    QString m_feedbackText;
    FeedbackKind m_feedbackKind = FeedbackKind::None;
};
