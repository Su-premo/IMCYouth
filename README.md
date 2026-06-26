# IMCYouth — 출석 및 회계 통합 관리 프로그램

> A desktop management application for youth ministry,  
> built with **Qt6 (C++)** and **SQLite**.

---

## Features

### 📋 출석 관리
- 월별 토요일 / 주일 1부·2부 출석 체크
- 출석률 파이차트 (토요일 / 1부 / 2부)
- 연간 월별 출석률 막대그래프
- 전체현황 팝업 (연간 멤버별 출석 현황)

### 💰 회계 관리
- 날짜 / 계정과목 / 적요 / 거래처 / 수입 / 지출 / 잔액 / 비고
- 영수증 이미지 첨부 및 열람
- 수입·지출 파이차트 / 연간 막대그래프
- **출납대장 Excel 내보내기** (상반기·하반기·전체, QXlsx)
- 🔒 BREAK 모드 — 인가된 임원 코드로 진입, 별도 내부 장부 관리

### 👥 명단 관리
- 멤버 정보 등록·수정·삭제 (이름 / 생년월일 / 연락처 / 주소 / 소속 / 비고)
- 이름 검색
- **3개월 이상 장기 미출석자 자동 분리** 표시

### ⚙️ 설정
- BREAK 모드 진입 감사 로그 (누가 언제 진입했는지 기록)
- 프로그램 로그인 비밀번호 변경

---

## Tech Stack

| | |
|---|---|
| Language | C++17 |
| Framework | Qt 6 |
| Database | SQLite (via Qt SQL) |
| Charts | Qt Charts |
| Excel Export | [QXlsx](https://github.com/QtExcel/QXlsx) |
| Build | CMake |
| Platform | Windows / Linux (Ubuntu) |

---

## Getting Started

### Requirements
- Qt 6.x
- CMake 3.5+
- QXlsx (see below)

### Install QXlsx
```bash
git clone https://github.com/QtExcel/QXlsx.git QXlsx/QXlsx
