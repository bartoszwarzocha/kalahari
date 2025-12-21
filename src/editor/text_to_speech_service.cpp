/// @file text_to_speech_service.cpp
/// @brief Text-to-Speech service implementation (OpenSpec #00042 Phase 6.21-6.24)

#include <kalahari/editor/text_to_speech_service.h>
#include <kalahari/editor/kml_document.h>
#include <kalahari/editor/kml_paragraph.h>
#include <kalahari/core/logger.h>

// Qt TextToSpeech is optional - check at compile time
#ifdef QT_TEXTTOSPEECH_LIB
#include <QTextToSpeech>
#include <QVoice>
#endif

#include <QTimer>
#include <QRegularExpression>

namespace kalahari::editor {

// =============================================================================
// TextToSpeechService
// =============================================================================

TextToSpeechService::TextToSpeechService(QObject* parent)
    : QObject(parent)
{
    initializeTts();

    if (m_available) {
        buildVoiceList();
        core::Logger::getInstance().info("TextToSpeechService: TTS engine initialized with {} voices",
                                         m_voices.size());
    } else {
        core::Logger::getInstance().info("TextToSpeechService: TTS not available - {}",
                                         m_errorMessage.toStdString());
    }
}

TextToSpeechService::~TextToSpeechService()
{
    stop();
    cleanupTts();
    core::Logger::getInstance().debug("TextToSpeechService destroyed");
}

// =============================================================================
// Availability
// =============================================================================

bool TextToSpeechService::isAvailable() const
{
    return m_available;
}

QString TextToSpeechService::errorMessage() const
{
    return m_errorMessage;
}

// =============================================================================
// Voice Selection
// =============================================================================

QList<VoiceInfo> TextToSpeechService::availableVoices() const
{
    return m_voices;
}

QList<VoiceInfo> TextToSpeechService::voicesForLanguage(const QString& language) const
{
    QList<VoiceInfo> result;

    // Match by language code (e.g., "en" matches "en-US", "en-GB")
    QString langPrefix = language.toLower();
    if (langPrefix.contains('-') || langPrefix.contains('_')) {
        // Exact match for full locale
        langPrefix = langPrefix.left(langPrefix.indexOf(QRegularExpression("[-_]")));
    }

    for (const VoiceInfo& voice : m_voices) {
        QString voiceLang = voice.language.toLower();
        if (voiceLang.startsWith(langPrefix)) {
            result.append(voice);
        }
    }

    return result;
}

void TextToSpeechService::setVoice(const QString& voiceId)
{
    if (!m_available || voiceId == m_currentVoiceId) {
        return;
    }

#ifdef QT_TEXTTOSPEECH_LIB
    auto* tts = static_cast<QTextToSpeech*>(m_tts);

    // Find voice by ID
    for (const QVoice& voice : tts->availableVoices()) {
        if (voice.name() == voiceId) {
            tts->setVoice(voice);
            m_currentVoiceId = voiceId;
            core::Logger::getInstance().debug("TextToSpeechService: Voice set to '{}'",
                                              voiceId.toStdString());
            return;
        }
    }

    core::Logger::getInstance().warn("TextToSpeechService: Voice '{}' not found",
                                     voiceId.toStdString());
#else
    Q_UNUSED(voiceId);
#endif
}

VoiceInfo TextToSpeechService::currentVoice() const
{
    for (const VoiceInfo& voice : m_voices) {
        if (voice.id == m_currentVoiceId) {
            return voice;
        }
    }
    return VoiceInfo();
}

// =============================================================================
// Playback Control
// =============================================================================

void TextToSpeechService::speak(const QString& text)
{
    if (!m_available) {
        core::Logger::getInstance().warn("TextToSpeechService: speak() called but TTS not available");
        emit error(m_errorMessage);
        return;
    }

    if (text.isEmpty()) {
        return;
    }

    // Cancel document reading mode
    m_readingDocument = false;
    m_document = nullptr;
    m_currentText = text;
    m_textOffset = 0;

#ifdef QT_TEXTTOSPEECH_LIB
    auto* tts = static_cast<QTextToSpeech*>(m_tts);
    tts->say(text);
#endif
}

void TextToSpeechService::speakFromDocument(KmlDocument* document, int startParagraph)
{
    if (!m_available) {
        core::Logger::getInstance().warn("TextToSpeechService: speakFromDocument() called but TTS not available");
        emit error(m_errorMessage);
        return;
    }

    if (!document || document->paragraphCount() == 0) {
        core::Logger::getInstance().warn("TextToSpeechService: No document or empty document");
        return;
    }

    // Validate start paragraph
    if (startParagraph < 0 || startParagraph >= document->paragraphCount()) {
        startParagraph = 0;
    }

    m_document = document;
    m_currentParagraph = startParagraph;
    m_readingDocument = true;

    core::Logger::getInstance().debug("TextToSpeechService: Starting document reading from paragraph {}",
                                      startParagraph);

    // Start reading first paragraph
    continueDocumentReading();
}

void TextToSpeechService::pause()
{
    if (!m_available || m_state != TtsState::Speaking) {
        return;
    }

#ifdef QT_TEXTTOSPEECH_LIB
    auto* tts = static_cast<QTextToSpeech*>(m_tts);
    tts->pause();
#endif
}

void TextToSpeechService::resume()
{
    if (!m_available || m_state != TtsState::Paused) {
        return;
    }

#ifdef QT_TEXTTOSPEECH_LIB
    auto* tts = static_cast<QTextToSpeech*>(m_tts);
    tts->resume();
#endif
}

void TextToSpeechService::stop()
{
    if (!m_available) {
        return;
    }

    // Cancel document reading
    m_readingDocument = false;
    m_document = nullptr;

#ifdef QT_TEXTTOSPEECH_LIB
    auto* tts = static_cast<QTextToSpeech*>(m_tts);
    tts->stop();
#endif

    setState(TtsState::Idle);
}

TtsState TextToSpeechService::state() const
{
    return m_state;
}

// =============================================================================
// Settings
// =============================================================================

void TextToSpeechService::setRate(double rate)
{
    // Clamp to valid range
    m_rate = qBound(-1.0, rate, 1.0);

#ifdef QT_TEXTTOSPEECH_LIB
    if (m_available) {
        auto* tts = static_cast<QTextToSpeech*>(m_tts);
        tts->setRate(m_rate);
    }
#endif
}

double TextToSpeechService::rate() const
{
    return m_rate;
}

void TextToSpeechService::setPitch(double pitch)
{
    // Clamp to valid range
    m_pitch = qBound(-1.0, pitch, 1.0);

#ifdef QT_TEXTTOSPEECH_LIB
    if (m_available) {
        auto* tts = static_cast<QTextToSpeech*>(m_tts);
        tts->setPitch(m_pitch);
    }
#endif
}

double TextToSpeechService::pitch() const
{
    return m_pitch;
}

void TextToSpeechService::setVolume(double volume)
{
    // Clamp to valid range
    m_volume = qBound(0.0, volume, 1.0);

#ifdef QT_TEXTTOSPEECH_LIB
    if (m_available) {
        auto* tts = static_cast<QTextToSpeech*>(m_tts);
        tts->setVolume(m_volume);
    }
#endif
}

double TextToSpeechService::volume() const
{
    return m_volume;
}

// =============================================================================
// Private Slots
// =============================================================================

void TextToSpeechService::onEngineStateChanged(int state)
{
#ifdef QT_TEXTTOSPEECH_LIB
    QTextToSpeech::State ttsState = static_cast<QTextToSpeech::State>(state);

    switch (ttsState) {
    case QTextToSpeech::Ready:
        // Check if we were speaking and now finished
        if (m_state == TtsState::Speaking) {
            // If reading document, continue to next paragraph
            if (m_readingDocument && m_document) {
                m_currentParagraph++;
                if (m_currentParagraph < m_document->paragraphCount()) {
                    // Use timer to avoid recursion
                    QTimer::singleShot(100, this, &TextToSpeechService::continueDocumentReading);
                    return;
                } else {
                    // Document reading complete
                    m_readingDocument = false;
                    m_document = nullptr;
                }
            }
            setState(TtsState::Idle);
            emit finished();
        } else {
            setState(TtsState::Idle);
        }
        break;

    case QTextToSpeech::Speaking:
        if (m_state != TtsState::Speaking) {
            setState(TtsState::Speaking);
            emit started();
        }
        break;

    case QTextToSpeech::Paused:
        setState(TtsState::Paused);
        break;

    case QTextToSpeech::Error:
        setState(TtsState::Error);
        m_readingDocument = false;
        m_document = nullptr;
        {
            auto* tts = static_cast<QTextToSpeech*>(m_tts);
            QString errorMsg = tr("TTS engine error");
            // QTextToSpeech::errorString() available in Qt 6.4+
            core::Logger::getInstance().error("TextToSpeechService: Engine error");
            emit error(errorMsg);
        }
        break;
    }
#else
    Q_UNUSED(state);
#endif
}

void TextToSpeechService::continueDocumentReading()
{
    if (!m_readingDocument || !m_document || !m_available) {
        return;
    }

    if (m_currentParagraph >= m_document->paragraphCount()) {
        m_readingDocument = false;
        m_document = nullptr;
        setState(TtsState::Idle);
        emit finished();
        return;
    }

    const KmlParagraph* para = m_document->paragraph(m_currentParagraph);
    if (!para) {
        m_currentParagraph++;
        continueDocumentReading();
        return;
    }

    QString text = para->plainText().trimmed();
    if (text.isEmpty()) {
        // Skip empty paragraphs
        m_currentParagraph++;
        continueDocumentReading();
        return;
    }

    m_currentText = text;
    m_textOffset = 0;

    emit paragraphStarted(m_currentParagraph);

    core::Logger::getInstance().debug("TextToSpeechService: Reading paragraph {} of {}",
                                      m_currentParagraph + 1, m_document->paragraphCount());

#ifdef QT_TEXTTOSPEECH_LIB
    auto* tts = static_cast<QTextToSpeech*>(m_tts);
    tts->say(text);
#endif
}

// =============================================================================
// Private Methods
// =============================================================================

void TextToSpeechService::initializeTts()
{
#ifdef QT_TEXTTOSPEECH_LIB
    auto* tts = new QTextToSpeech(this);

    // Check if engine is ready
    if (tts->state() == QTextToSpeech::Error) {
        m_errorMessage = tr("Text-to-Speech engine initialization failed");
        m_available = false;
        delete tts;
        return;
    }

    // Check available engines
    QStringList engines = QTextToSpeech::availableEngines();
    if (engines.isEmpty()) {
        m_errorMessage = tr("No Text-to-Speech engines available on this system");
        m_available = false;
        delete tts;
        return;
    }

    m_tts = tts;
    m_available = true;

    // Connect state changed signal
    connect(tts, &QTextToSpeech::stateChanged,
            this, [this](QTextToSpeech::State state) {
                onEngineStateChanged(static_cast<int>(state));
            });

    // Connect word boundary signal if available (Qt 6.4+)
    // Note: sayingWord signal added in Qt 6.6
#if QT_VERSION >= QT_VERSION_CHECK(6, 6, 0)
    connect(tts, &QTextToSpeech::sayingWord,
            this, [this](const QString& /*word*/, qsizetype start, qsizetype length) {
                emit wordBoundary(static_cast<int>(start + m_textOffset),
                                  static_cast<int>(length));
            });
#endif

    // Apply default settings
    tts->setRate(m_rate);
    tts->setPitch(m_pitch);
    tts->setVolume(m_volume);

    core::Logger::getInstance().debug("TextToSpeechService: Using engine '{}', available engines: {}",
                                      tts->engine().toStdString(),
                                      engines.join(", ").toStdString());
#else
    m_errorMessage = tr("Text-to-Speech not compiled in this build. "
                        "Qt TextToSpeech module is required.");
    m_available = false;

    core::Logger::getInstance().info("TextToSpeechService: Qt TextToSpeech module not available");
#endif
}

void TextToSpeechService::buildVoiceList()
{
#ifdef QT_TEXTTOSPEECH_LIB
    if (!m_tts) {
        return;
    }

    auto* tts = static_cast<QTextToSpeech*>(m_tts);
    m_voices.clear();

    for (const QVoice& voice : tts->availableVoices()) {
        VoiceInfo info;
        info.id = voice.name();
        info.name = voice.name();
        info.language = tts->locale().name();  // e.g., "en_US"

        switch (voice.gender()) {
        case QVoice::Male:
            info.gender = "Male";
            break;
        case QVoice::Female:
            info.gender = "Female";
            break;
        default:
            info.gender = "Unknown";
            break;
        }

        m_voices.append(info);
    }

    // Set current voice to first available if any
    if (!m_voices.isEmpty() && m_currentVoiceId.isEmpty()) {
        m_currentVoiceId = m_voices.first().id;
    }
#endif
}

void TextToSpeechService::cleanupTts()
{
#ifdef QT_TEXTTOSPEECH_LIB
    if (m_tts) {
        auto* tts = static_cast<QTextToSpeech*>(m_tts);
        tts->stop();
        // QTextToSpeech is parented to this, will be deleted automatically
        m_tts = nullptr;
    }
#endif
    m_available = false;
    m_voices.clear();
}

void TextToSpeechService::setState(TtsState newState)
{
    if (m_state != newState) {
        m_state = newState;
        emit stateChanged(newState);
    }
}

}  // namespace kalahari::editor
