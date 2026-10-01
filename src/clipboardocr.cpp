#include "include/clipboardocr.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QPointer>
#include <QImage>
#include <QPainter>
#include <QBuffer>
#include <windows.h>
#include <appmodel.h>
#include <winrt/Windows.ApplicationModel.DataTransfer.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Imaging.h>
#include <winrt/Windows.Media.Ocr.h>
#include <winrt/Windows.Storage.Streams.h>

namespace {
winrt::fire_and_forget recognizeHistory(QPointer<ClipboardOcr> owner, QString id, quint64 generation, QByteArray fixture = {})
{
    QString text, status;
    try {
        using namespace winrt::Windows::ApplicationModel::DataTransfer;
        using namespace winrt::Windows::Graphics::Imaging;
        using winrt::Windows::Media::Ocr::OcrEngine;
        // The clipboard APIs are called in the initiating STA. Decode/OCR move
        // to a worker apartment; no image bytes or recognized text are logged.
        winrt::Windows::Storage::Streams::IRandomAccessStream stream{nullptr};
        if (!fixture.isEmpty()) {
            winrt::Windows::Storage::Streams::InMemoryRandomAccessStream memory;
            winrt::Windows::Storage::Streams::DataWriter writer(memory);
            writer.WriteBytes(winrt::array_view<const uint8_t>(reinterpret_cast<const uint8_t *>(fixture.constData()), reinterpret_cast<const uint8_t *>(fixture.constData()) + fixture.size()));
            co_await writer.StoreAsync(); writer.DetachStream(); memory.Seek(0); stream = memory;
        } else {
            const auto history = co_await Clipboard::GetHistoryItemsAsync();
            if (history.Status() != ClipboardHistoryItemsResultStatus::Success)
                throw std::runtime_error("history-unavailable");
            for (const auto &item : history.Items()) {
                if (QString::fromStdWString(item.Id().c_str()) != id) continue;
                if (!item.Content().Contains(StandardDataFormats::Bitmap())) break;
                const auto reference = co_await item.Content().GetBitmapAsync();
                stream = co_await reference.OpenReadAsync();
                break;
            }
        }
        if (!stream) {
            status = QStringLiteral("这张图片已不在 Windows 历史中，请刷新后重试");
        } else if (stream.Size() == 0 || stream.Size() > 12 * 1024 * 1024) {
            status = QStringLiteral("图片为空或超过 12 MB，未进行识别");
        } else {
            co_await winrt::resume_background();
            const auto engine = OcrEngine::TryCreateFromUserProfileLanguages();
            if (!engine) {
                status = QStringLiteral("没有可用的 OCR 语言，请在 Windows 语言设置中安装语言支持");
            } else {
                const auto decoder = co_await BitmapDecoder::CreateAsync(stream);
                const auto limit = OcrEngine::MaxImageDimension();
                if (decoder.PixelWidth() > limit || decoder.PixelHeight() > limit) {
                    status = QStringLiteral("原图超过 Windows OCR 的尺寸限制 %1 像素").arg(limit);
                } else {
                    const auto bitmap = co_await decoder.GetSoftwareBitmapAsync(BitmapPixelFormat::Bgra8, BitmapAlphaMode::Premultiplied);
                    const auto result = co_await engine.RecognizeAsync(bitmap);
                    QStringList lines;
                    for (const auto &line : result.Lines())
                        lines.append(QString::fromStdWString(line.Text().c_str()));
                    text = lines.join('\n');
                    const bool truncated = text.size() > 65536;
                    text = text.left(65536);
                    status = text.isEmpty() ? QStringLiteral("未识别到文字")
                        : truncated ? QStringLiteral("识别完成，结果已截断至 65536 字符")
                        : QStringLiteral("本地识别完成 · 可编辑后复制");
                }
            }
        }
    } catch (const winrt::hresult_error &error) {
        status = QStringLiteral("Windows OCR 失败（0x%1），请检查语言支持和应用权限")
            .arg(static_cast<quint32>(error.code().value), 8, 16, QLatin1Char('0'));
    } catch (...) {
        status = QStringLiteral("无法读取图片历史，请检查 Windows 剪贴板历史是否启用");
    }
    QMetaObject::invokeMethod(qGuiApp, [owner, generation, text, status] {
        if (owner) owner->finish(generation, text, status);
    }, Qt::QueuedConnection);
}
}

ClipboardOcr::ClipboardOcr(QObject *parent, std::function<void(const QString &)> copySink)
    : QObject(parent), m_copySink(std::move(copySink))
{
    if (!m_copySink) m_copySink = [](const QString &text) { QGuiApplication::clipboard()->setText(text); };
}

void ClipboardOcr::recognize(const QString &historyId)
{
    if (m_busy || historyId.isEmpty()) return;
    UINT32 length = 0;
    if (GetCurrentPackageFullName(&length, nullptr) == APPMODEL_ERROR_NO_PACKAGE) {
        m_status = QStringLiteral("Windows OCR 需要 MSIX 应用身份，请使用打包安装版本");
        m_text.clear();
        emit textChanged();
        emit changed();
        return;
    }
    m_busy = true;
    m_text.clear();
    emit textChanged();
    m_status = QStringLiteral("正在本地识别原图…");
    emit changed();
    recognizeHistory(this, historyId, ++m_generation);
}

void ClipboardOcr::finish(quint64 generation, const QString &text, const QString &status)
{
    if (generation != m_generation) return;
    m_busy = false; m_text = text; m_status = status;
    emit textChanged();
    emit changed();
}

void ClipboardOcr::recognizeFixture(const QString &kind)
{
    bool enabled = false;
    for (const auto &argument : qGuiApp->arguments())
        enabled |= argument.startsWith("--feature-self-test=");
    if (!enabled || m_busy) return;
    QByteArray bytes;
    if (kind == "malformed") bytes = "not-a-valid-image";
    else if (kind == "bytes") bytes = QByteArray(12 * 1024 * 1024 + 1, '0');
    else {
        const int width = kind == "dimensions" ? static_cast<int>(winrt::Windows::Media::Ocr::OcrEngine::MaxImageDimension()) + 1 : 900;
        QImage image(width, 120, QImage::Format_RGB32); image.fill(Qt::white);
        if (kind == "text") {
            QPainter painter(&image); painter.setPen(Qt::black);
            QFont font("Arial"); font.setPixelSize(42); painter.setFont(font);
            painter.drawText(20, 78, "WINDOWS OCR TEST 123"); painter.end();
        }
        QBuffer buffer(&bytes); buffer.open(QIODevice::WriteOnly); image.save(&buffer, "PNG");
    }
    m_busy = true; m_text.clear(); m_status = QStringLiteral("识别生成的测试图");
    emit textChanged(); emit changed();
    recognizeHistory(this, {}, ++m_generation, bytes);
}

void ClipboardOcr::clear()
{
    // Keep a single operation in flight; hiding a result is not cancellation.
    if (m_busy) return;
    m_text.clear(); m_status.clear(); emit changed();
    emit textChanged();
}

void ClipboardOcr::copy(const QString &text)
{
    if (m_busy || text.isEmpty() || text.size() > 65536) return;
    m_copySink(text);
    m_status = QStringLiteral("识别文字已复制"); emit changed();
}
