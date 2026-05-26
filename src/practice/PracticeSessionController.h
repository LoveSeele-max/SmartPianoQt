#pragma once

#include "practice/PracticeEngine.h"
#include "storage/PracticeRecordStore.h"

#include <QElapsedTimer>
#include <QString>

class PracticeSessionController {
public:
    explicit PracticeSessionController(PracticeRecordStore *store = nullptr);

    void setRecordStore(PracticeRecordStore *store);
    void setSheetId(qint64 sheetId);
    qint64 sheetId() const { return m_sheetId; }
    qint64 sessionId() const { return m_sessionId; }
    bool isActive() const { return m_active; }

    bool begin(const QString &mode, int playbackSpeed, qint64 startTick);
    void pause();
    void resume();
    bool append(const PracticeNoteResult &result, qint64 fallbackActualTick, int offsetMs);
    bool finish(bool completed, qint64 endTick, int correctCount, int wrongCount, int missedCount);

private:
    PracticeRecordStore *m_store = nullptr;
    qint64 m_sheetId = -1;
    qint64 m_sessionId = -1;
    bool m_active = false;
    bool m_activeClockRunning = false;
    qint64 m_activeElapsedMs = 0;
    QElapsedTimer m_activeTimer;
};
