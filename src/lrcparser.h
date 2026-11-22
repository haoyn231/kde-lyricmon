// SPDX-FileCopyrightText: 2026 haoyn231
// SPDX-License-Identifier: MIT
//
// LRC 解析与歌词匹配评分。
//
// 这里刻意不依赖 QtDBus / QNetwork，纯函数式，方便直接单元测试：
//   parse()   —— 一段文本进，歌词结构出（同步 / 纯文本两种结果）
//   normalize()/score() —— LRCLIB 搜索结果评分，决定用哪一条

#pragma once

#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QVector>

struct LyricLine {
    qint64 timeMs = -1;   // -1 表示这一行没有时间轴（纯文本歌词）
    QString text;
};

struct Lyrics {
    QVector<LyricLine> synced;   // 按时间升序；空 = 没有可同步的歌词
    QString plain;               // 无时间戳的整段歌词
    bool instrumental = false;   // 纯音乐（LRCLIB 会明确标出来）

    bool isEmpty() const { return synced.isEmpty() && plain.trimmed().isEmpty(); }
    bool isSynced() const { return !synced.isEmpty(); }
};

namespace Lrc {

// 解析一段歌词文本。带时间戳的按时间排序；没有时间戳的整体当纯文本。
Lyrics parse(const QString &raw);

// 用来做匹配的归一化形式：小写、去括号补充说明、去标点、压空白。
QString normalize(const QString &text);

// 给 LRCLIB 的候选结果打分，0 表示根本不匹配。满分 100 上下。
int score(const QString &title, const QStringList &artists, const QString &album,
          qint64 durationMs, const QJsonObject &candidate);

// 去掉逐字标签 <00:12.34>，只保留可读文本。
QString stripWordTags(const QString &text);

} // namespace Lrc
