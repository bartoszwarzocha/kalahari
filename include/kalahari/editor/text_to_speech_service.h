/// @file text_to_speech_service.h
/// @brief Text-to-Speech service for reading documents aloud (OpenSpec #00042 Phase 6.21-6.24)
///
/// TextToSpeechService provides:
/// - Text-to-speech synthesis using Qt TextToSpeech (when available)
/// - Graceful degradation when TTS is not available
/// - Play/pause/resume/stop controls
/// - Voice selection and settings (rate, pitch, volume)
/// - Word boundary signals for highlighting
/// - Document reading with paragraph tracking
///
/// The service uses an optional Qt TextToSpeech backend. If the module is not
/// compiled in or the platform doesn't support TTS, the service degrades
/// gracefully and reports unavailability.

#pragma once

#include <QObject>
#include <QString>
#include <QStringList>
#include <QList>

namespace kalahari::editor {

/// @brief Playback state for TTS
enum class TtsState {
    Idle,       ///< Not playing
    Speaking,   ///< Currently speaking
    Paused,     ///< Paused mid-speech
    Error       ///< Error state (TTS unavailable)
};

/// @brief Information about a voice
struct VoiceInfo {
    QString id;         ///< Voice identifier
    QString name;       ///< Display name
    QString language;   ///< Language code (e.g., "en-US", "pl-PL")
    QString gender;     ///< "Male", "Female", or "Unknown"

    VoiceInfo() = default;
    VoiceInfo(const QString& voiceId, const QString& voiceName,
              const QString& lang, const QString& gen)
        : id(voiceId), name(voiceName), language(lang), gender(gen) {}

    bool operator==(const VoiceInfo& other) const {
        return id == other.id;
    }
};

/// @brief Text-to-Speech service using Qt Speech (optional)
///
/// Provides text-to-speech functionality for reading documents aloud.
/// The service is designed to work without Qt TextToSpeech module -
/// in that case it gracefully reports unavailability.
///
/// Usage:
/// @code
/// auto tts = new TextToSpeechService(this);
/// if (tts->isAvailable()) {
///     tts->setVolume(0.8);
///     tts->speak("Hello, world!");
///
///     connect(tts, &TextToSpeechService::wordBoundary,
///             editor, &BookEditor::highlightWord);
/// }
/// @endcode
class TextToSpeechService : public QObject {
    Q_OBJECT

public:
    /// @brief Construct TTS service
    /// @param parent Parent QObject for ownership
    explicit TextToSpeechService(QObject* parent = nullptr);

    /// @brief Destructor - cleans up TTS engine
    ~TextToSpeechService() override;

    // Non-copyable
    TextToSpeechService(const TextToSpeechService&) = delete;
    TextToSpeechService& operator=(const TextToSpeechService&) = delete;

    // =========================================================================
    // Availability
    // =========================================================================

    /// @brief Check if TTS is available on this system
    /// @return true if TTS engine is ready
    bool isAvailable() const;

    /// @brief Get error message if not available
    /// @return Error message or empty string if available
    QString errorMessage() const;

    // =========================================================================
    // Voice Selection
    // =========================================================================

    /// @brief Get list of available voices
    /// @return List of voice information structs
    QList<VoiceInfo> availableVoices() const;

    /// @brief Get voices for specific language
    /// @param language Language code (e.g., "en", "pl", "en-US")
    /// @return List of voices matching the language
    QList<VoiceInfo> voicesForLanguage(const QString& language) const;

    /// @brief Set current voice by ID
    /// @param voiceId Voice identifier from VoiceInfo::id
    void setVoice(const QString& voiceId);

    /// @brief Get current voice
    /// @return Current voice information
    VoiceInfo currentVoice() const;

    // =========================================================================
    // Playback Control
    // =========================================================================

    /// @brief Speak the given text
    /// @param text Text to synthesize and speak
    void speak(const QString& text);

    /// @brief Pause playback
    void pause();

    /// @brief Resume paused playback
    void resume();

    /// @brief Stop playback completely
    void stop();

    /// @brief Get current playback state
    /// @return Current TTS state
    TtsState state() const;

    // =========================================================================
    // Settings
    // =========================================================================

    /// @brief Set speech rate
    /// @param rate Rate from -1.0 (slowest) to 1.0 (fastest), 0.0 = normal
    void setRate(double rate);

    /// @brief Get current speech rate
    /// @return Rate from -1.0 to 1.0
    double rate() const;

    /// @brief Set pitch
    /// @param pitch Pitch from -1.0 (lowest) to 1.0 (highest), 0.0 = normal
    void setPitch(double pitch);

    /// @brief Get current pitch
    /// @return Pitch from -1.0 to 1.0
    double pitch() const;

    /// @brief Set volume
    /// @param volume Volume from 0.0 (mute) to 1.0 (full)
    void setVolume(double volume);

    /// @brief Get current volume
    /// @return Volume from 0.0 to 1.0
    double volume() const;

signals:
    /// @brief Emitted when playback state changes
    /// @param newState The new TTS state
    void stateChanged(TtsState newState);

    /// @brief Emitted when speaking starts
    void started();

    /// @brief Emitted when speaking finishes (not paused, but completed/stopped)
    void finished();

    /// @brief Emitted with current word being spoken (for highlighting)
    /// @param startPos Start position of word in current text
    /// @param length Length of word in characters
    void wordBoundary(int startPos, int length);

    /// @brief Emitted on error
    /// @param message Error description
    void error(const QString& message);

private slots:
    /// @brief Handle TTS engine state change
    /// @param state New state as int (platform-specific)
    void onEngineStateChanged(int state);

private:
    /// @brief Initialize TTS engine
    void initializeTts();

    /// @brief Build voice list from engine
    void buildVoiceList();

    /// @brief Clean up TTS engine
    void cleanupTts();

    /// @brief Update internal state and emit signal
    /// @param newState New state to set
    void setState(TtsState newState);

    // =========================================================================
    // Members
    // =========================================================================

    /// @brief QTextToSpeech instance (opaque pointer to avoid header dependency)
    /// @note nullptr if Qt TextToSpeech module not available
    void* m_tts{nullptr};

    /// @brief Availability flag
    bool m_available{false};

    /// @brief Error message if not available
    QString m_errorMessage;

    /// @brief Current playback state
    TtsState m_state{TtsState::Idle};

    /// @brief Cached voice list
    QList<VoiceInfo> m_voices;

    /// @brief Current voice ID
    QString m_currentVoiceId;

    /// @brief Speech rate (-1.0 to 1.0)
    double m_rate{0.0};

    /// @brief Speech pitch (-1.0 to 1.0)
    double m_pitch{0.0};

    /// @brief Speech volume (0.0 to 1.0)
    double m_volume{1.0};

    /// @brief Current text being spoken (for word boundary calculation)
    QString m_currentText;

    /// @brief Text offset for word boundary calculation
    int m_textOffset{0};
};

}  // namespace kalahari::editor
