#pragma once
#include <QObject>
#include <QProcess>
#include <QElapsedTimer>
#include <QStringConverter>

class TerminalManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString output READ output NOTIFY outputChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString sandboxPath READ sandboxPath CONSTANT)
    Q_PROPERTY(QString shellName READ shellName CONSTANT)
    Q_PROPERTY(QString logPath READ logPath CONSTANT)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
public:
    explicit TerminalManager(QObject *parent = nullptr);
    QString output() const { return m_output; }
    QString statusText() const { return m_statusText; }
    QString sandboxPath() const { return m_sandboxPath; }
    QString shellName() const { return m_shellName; }
    QString logPath() const { return m_logPath; }
    bool running() const { return m_process.state() != QProcess::NotRunning; }
    Q_INVOKABLE void run(const QString &command);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void openLogFile();
signals:
    void outputChanged();
    void statusChanged();
    void runningChanged();
private:
    void appendOutput(const QByteArray &data);
    void writeLog(const QString &event, const QString &detail = {});
    QProcess m_process;
    QString m_output;
    QString m_statusText = QStringLiteral("受限 Runner 已就绪");
    QString m_sandboxPath;
    QString m_shellProgram;
    QString m_shellName;
    QString m_logPath;
    QStringDecoder m_utf8Decoder{QStringConverter::Utf8};
    QElapsedTimer m_runTimer;
    qsizetype m_outputBytes = 0;
};
