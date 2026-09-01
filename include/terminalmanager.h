#pragma once
#include <QObject>
#include <QProcess>
#include <QElapsedTimer>
#include <QStringConverter>
#include <QStringList>
#include <QHash>

class TerminalManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString output READ output NOTIFY outputChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusChanged)
    Q_PROPERTY(QString sandboxPath READ sandboxPath CONSTANT)
    Q_PROPERTY(QString currentDirectory READ currentDirectory NOTIFY currentDirectoryChanged)
    Q_PROPERTY(QString shellName READ shellName CONSTANT)
    Q_PROPERTY(QString logPath READ logPath CONSTANT)
    Q_PROPERTY(bool running READ running NOTIFY runningChanged)
    Q_PROPERTY(int sessionEnvironmentCount READ sessionEnvironmentCount NOTIFY statusChanged)
public:
    explicit TerminalManager(QObject *parent = nullptr);
    QString output() const { return m_output; }
    QString statusText() const { return m_statusText; }
    QString sandboxPath() const { return m_sandboxPath; }
    QString currentDirectory() const { return m_currentDirectory; }
    QString shellName() const { return m_shellName; }
    QString logPath() const { return m_logPath; }
    bool running() const { return m_process.state() != QProcess::NotRunning; }
    int sessionEnvironmentCount() const { return m_sessionEnvironment.size(); }
    Q_INVOKABLE void run(const QString &command);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void openLogFile();
    Q_INVOKABLE QStringList completionSuggestions(const QString &input) const;
signals:
    void outputChanged();
    void statusChanged();
    void runningChanged();
    void currentDirectoryChanged();
private:
    void appendOutput(const QByteArray &data);
    QString stripTerminalSequences(const QString &text);
    void writeLog(const QString &event, const QString &detail = {});
    QProcess m_process;
    QString m_output;
    QString m_statusText = QStringLiteral("受限 Runner 已就绪");
    QString m_sandboxPath;
    QString m_currentDirectory;
    QString m_shellProgram;
    QString m_shellName;
    QString m_logPath;
    QStringList m_availableCommands;
    QHash<QString, QString> m_sessionEnvironment;
    QStringDecoder m_utf8Decoder{QStringConverter::Utf8};
    QElapsedTimer m_runTimer;
    qsizetype m_outputBytes = 0;
    enum class EscapeState { Normal, Escape, Csi, Osc, OscEscape };
    EscapeState m_escapeState = EscapeState::Normal;
};
