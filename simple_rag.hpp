#pragma once
#include <QObject>
#include <QFile>
#include <QStringList>
#include <QVector>
#include <QRegExp>

class SimpleRAG : public QObject {
    Q_OBJECT
public:
    explicit SimpleRAG(QObject* parent = nullptr) : QObject(parent) {}
    bool loadDocument(const QString& path) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return false;
        m_lines = QString::fromUtf8(f.readAll()).split('\n');
        for (int i = m_lines.size()-1; i >= 0; --i)
            if (m_lines[i].trimmed().isEmpty()) m_lines.removeAt(i);
        return true;
    }
    QString retrieve(const QString& query, int maxMatches = 5) const {
        if (query.trimmed().isEmpty() || m_lines.isEmpty()) return QString();
        QString qLower = query.toLower();
        QStringList qTokens = qLower.split(QRegExp("\\W+"), Qt::SkipEmptyParts);

        struct Hit { int score; QString text; };
        QVector<Hit> hits;
        for (const QString& line : m_lines) {
            QString lower = line.toLower();
            int score = 0;
            for (const QString& t : qTokens) {
                if (t.length() < 2) continue;
                if (lower.contains(t)) ++score;
            }
            if (score > 0) hits.push_back({score, line});
        }
        if (hits.empty()) return QString();
        std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b){ return a.score > b.score; });
        QStringList out;
        for (int i = 0; i < std::min((int)hits.size(), maxMatches); ++i) out << hits[i].text;
        return out.join("\n");
    }
private:
    QStringList m_lines;
};
