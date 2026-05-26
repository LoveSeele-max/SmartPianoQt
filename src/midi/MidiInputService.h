#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

enum class MidiInputMessageType {
    Ignored,
    NoteOn,
    NoteOff,
    Other
};

struct MidiInputMessage {
    MidiInputMessageType type = MidiInputMessageType::Ignored;
    int midi = -1;
    int velocity = 0;
};

class MidiInputService : public QObject {
    Q_OBJECT
    Q_PROPERTY(QStringList inputPorts READ inputPorts NOTIFY portsChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString backendName READ backendName CONSTANT)

public:
    explicit MidiInputService(QObject *parent = nullptr);
    ~MidiInputService() override;

    QStringList inputPorts() const { return m_inputPorts; }
    QString statusText() const { return m_statusText; }
    QString backendName() const;

    Q_INVOKABLE void refreshPorts();
    Q_INVOKABLE void openPort(int index);
    Q_INVOKABLE void close();

    static MidiInputMessage decodeShortMessage(int status, int data1, int data2);

signals:
    void portsChanged();
    void noteOn(int midi, int velocity);
    void noteOff(int midi);
    void statusChanged();

private:
    void setStatusText(const QString &message);

    QStringList m_inputPorts;
    QString m_statusText;
    quintptr m_nativeHandle = 0;
    int m_openPortIndex = -1;
    QString m_openPortName;
};
