#ifndef CONFIG_H
#define CONFIG_H

//#pragma once
#include <QString>
#include <QDir>
#include <QCoreApplication>

// ===== 경로 설정 (Ubuntu ↔ Windows 전환 시 여기만 수정) =====
// Ubuntu 개발 환경
const QString BASE_PATH = QDir::homePath() + "/IMCYouth";
// Windows 배포 환경
// const QString BASE_PATH = QCoreApplication::applicationDirPath();
// =============================================================

#endif // CONFIG_H
