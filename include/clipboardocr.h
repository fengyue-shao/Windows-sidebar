#pragma once
#include <QObject>
#include <functional>

class ClipboardOcr : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString status READ status NOTIFY changed)
    Q_PROPERTY(QString text READ text NOTIFY textChanged)
public:
    explicit ClipboardOcr(QObject *parent = nullptr, std::function<void(const QString &)> copySink = {});
    bool busy() const { return m_busy; }
    QString status() const { return m_status; }
    QString text() const { return m_text; }
    Q_INVOKABLE void recognize(const QString &historyId);
    void recognizeFixture(const QString &kind = "text"); // Explicit CLI self-test only.
    Q_INVOKABLE void clear();
    Q_INVOKABLE void copy(const QString &text);
    void finish(quint64 generation, const QString &text, const QString &status);
signals:
    void changed();
    void textChanged();
private:
    bool m_busy = false;
    quint64 m_generation = 0;
    QString m_status, m_text;
    std::function<void(const QString &)> m_copySink;
};
