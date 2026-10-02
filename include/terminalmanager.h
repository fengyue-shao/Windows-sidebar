#pragma once
#include <QObject>
#include <QProcess>
#include <QElapsedTimer>
#include <QStringConverter>
#include <QStringList>
#include <QHash>
#include <QTimer>

class TerminalScreen;

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
    Q_PROPERTY(bool conPtyAvailable READ conPtyAvailable CONSTANT)
    Q_PROPERTY(QString transportName READ transportName NOTIFY transportChanged)
    Q_PROPERTY(bool transportInteractive READ transportInteractive NOTIFY transportChanged)
    Q_PROPERTY(QString preferredTransport READ preferredTransport WRITE setPreferredTransport NOTIFY transportChanged)
    Q_PROPERTY(QObject* screen READ screen CONSTANT)
    Q_PROPERTY(bool sessionActive READ sessionActive NOTIFY sessionChanged)
    Q_PROPERTY(bool sessionStarting READ sessionStarting NOTIFY sessionChanged)
    Q_PROPERTY(qint64 elapsedMilliseconds READ elapsedMilliseconds NOTIFY elapsedChanged)
    Q_PROPERTY(int commandTimeoutSeconds READ commandTimeoutSeconds WRITE setCommandTimeoutSeconds NOTIFY statusChanged)
public:
    explicit TerminalManager(QObject *parent = nullptr, const QString &dataRoot = {});
    ~TerminalManager() override;
    QString output() const { return m_output; }
    QString statusText() const { return m_statusText; }
    QString sandboxPath() const { return m_sandboxPath; }
    QString currentDirectory() const { return m_currentDirectory; }
    QString shellName() const { return m_shellName; }
    QString logPath() const { return m_logPath; }
    bool running() const { return m_commandRunning; }
    int sessionEnvironmentCount() const { return m_sessionEnvironment.size(); }
    bool conPtyAvailable() const { return m_conPtyAvailable; }
    QString transportName() const { return m_transportName; }
    bool transportInteractive() const { return m_transportInteractive; }
    QString preferredTransport() const { return m_preferredTransport; }
    QObject *screen() const;
    bool sessionActive() const { return m_process.state() == QProcess::Running; }
    bool sessionStarting() const { return m_process.state() == QProcess::Starting; }
    qint64 elapsedMilliseconds() const;
    int commandTimeoutSeconds() const { return m_commandTimeoutSeconds; }
    Q_INVOKABLE void setPreferredTransport(const QString &transport);
    Q_INVOKABLE void setCommandTimeoutSeconds(int seconds);
    Q_INVOKABLE void startSession();
    Q_INVOKABLE void sendInput(const QString &input);
    Q_INVOKABLE void resizeTerminal(int columns, int rows);
    Q_INVOKABLE void run(const QString &command);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void clear();
    Q_INVOKABLE void resetSession();
    Q_INVOKABLE void openLogFile();
    Q_INVOKABLE QStringList completionSuggestions(const QString &input) const;
    Q_INVOKABLE int completionStart(const QString &input) const;
signals:
    void outputChanged();
    void statusChanged();
    void runningChanged();
    void currentDirectoryChanged();
    void commandCompleted(int exitCode, qint64 elapsedMs);
    void operationFinished(const QString &kind, const QString &outcome, int exitCode, qint64 elapsedMs);
    void transportChanged();
    void sessionChanged();
    void elapsedChanged();
private:
    void appendOutput(const QByteArray &data);
    void appendVisible(const QByteArray &data);
    void dispatchPendingInput();
    void finishCommand(const QString &outcome, int exitCode, bool naturalCompletion = false);
    void finishSession(const QString &outcome, int exitCode);
    void requestClose(const QString &outcome);
    void resetLocalState();
    bool ensureSession();
    QString stripTerminalSequences(const QString &text);
    void writeLog(const QString &event, const QString &detail = {});
    QProcess m_process;
    TerminalScreen *m_screen = nullptr;
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
    QElapsedTimer m_sessionTimer;
    QTimer m_commandTimeout;
    QTimer m_startTimeout;
    QTimer m_elapsedPulse;
    qint64 m_lastElapsed = 0;
    int m_commandTimeoutSeconds = 60;
    int m_columns = 80;
    int m_rows = 24;
    bool m_commandRunning = false;
    bool m_conPtyAvailable = false;
    bool m_transportInteractive = false;
    bool m_transportDecided = false;
    bool m_resetPending = false;
    bool m_sessionOutcomeEmitted = true;
    bool m_destroying = false;
    quint64 m_sessionGeneration = 0;
    QString m_preferredTransport = QStringLiteral("auto");
    QString m_terminationOutcome;
    QString m_pendingCommand;
    QByteArray m_pendingInput;
    QByteArray m_protocolPending;
    QByteArray m_commandMarker;
    QString m_transportName;
    qsizetype m_outputBytes = 0;
    enum class EscapeState { Normal, Escape, Csi, Osc, OscEscape };
    EscapeState m_escapeState = EscapeState::Normal;
};
