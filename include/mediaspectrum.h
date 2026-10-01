#pragma once
#include <QObject>
#include <QVariantList>
#include <memory>

class MediaSpectrum : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool requested READ requested WRITE setRequested NOTIFY requestedChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(bool available READ available NOTIFY spectrumChanged)
    Q_PROPERTY(QVariantList bands READ bands NOTIFY spectrumChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY spectrumChanged)
public:
    explicit MediaSpectrum(QObject *parent=nullptr);
    ~MediaSpectrum() override;
    bool requested() const { return m_requested; }
    bool enabled() const { return m_enabled; }
    bool available() const { return m_available; }
    QVariantList bands() const { return m_bands; }
    QString statusText() const { return m_status; }
    void setRequested(bool requested);
    void setEnabled(bool enabled);
signals:
    void requestedChanged();
    void enabledChanged();
    void spectrumChanged();
private:
    class CaptureThread;
    std::unique_ptr<CaptureThread> m_thread;
    bool m_requested=false, m_enabled=false, m_available=false;
    QVariantList m_bands;
    QString m_status;
};
