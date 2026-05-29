#ifndef CONFIG_H
#define CONFIG_H

//#pragma once
#include <QString>
#include <QDir>
#include <QCoreApplication>

// ===== 경로 설정 (Ubuntu ↔ Windows 전환 시 여기만 수정) =====
inline QString BASE_PATH() {
#ifdef Q_OS_WIN
    // Windows: 빌드 폴더 기준
    return QCoreApplication::applicationDirPath();
#else
    // Ubuntu: 홈 디렉토리 기준
    return QDir::homePath() + "/IMCYouth";
#endif
}

// =============================================================

#endif // CONFIG_H
