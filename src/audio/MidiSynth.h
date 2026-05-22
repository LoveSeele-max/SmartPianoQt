#pragma once

#include <QObject>
#include <QSet>
#include <QString>

class MidiSynth : public QObject {
    Q_OBJECT

public:
    explicit MidiSynth(QObject *parent = nullptr);
    ~MidiSynth() override;

    bool isAvailable() const { return m_available; }
    QString statusText() const { return m_statusText; }

    void noteOn(int midi, int velocity = 92);
    void noteOff(int midi);
    void stopAll();
    void setVolume(int volume);

private:
    void open();
    void close();
    void sendShortMessage(unsigned char status, unsigned char data1, unsigned char data2);
    void setProgram(int program);

    bool m_available = false;
    int m_volume = 110;
    QString m_statusText;
    QSet<int> m_soundingNotes;

#ifdef Q_OS_WIN
    void *m_output = nullptr;
#endif
};
