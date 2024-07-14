/*
 * Fooyin
 * Copyright © 2023, Luke Taylor <LukeT1@proton.me>
 *
 * Fooyin is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Fooyin is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Fooyin.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "application.h"

#include "corepaths.h"
#include "database/database.h"
#include "engine/enginehandler.h"
#include "engine/ffmpeg/ffmpegdecoder.h"
#include "internalcoresettings.h"
#include "library/librarymanager.h"
#include "library/sortingregistry.h"
#include "library/unifiedmusiclibrary.h"
#include "playlist/parsers/cueparser.h"
#include "playlist/parsers/m3uparser.h"
#include "playlist/playlistloader.h"
#include "plugins/pluginmanager.h"
#include "tagging/ffmpegparser.h"
#include "tagging/taglibparser.h"
#include "translations.h"

#include <core/engine/decoderprovider.h>
#include <core/engine/outputplugin.h>
#include <core/player/playercontroller.h>
#include <core/playlist/playlisthandler.h>
#include <core/plugins/coreplugin.h>
#include <core/tagging/tagloader.h>
#include <utils/settings/settingsmanager.h>

#include <QBasicTimer>
#include <QCoreApplication>
#include <QProcess>
#include <QTimerEvent>

using namespace std::chrono_literals;

constexpr auto LastPlaybackPosition = "Player/LastPositon";
constexpr auto LastPlaybackState    = "Player/LastState";

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
constexpr auto PlaylistSaveInterval = 30s;
constexpr auto SettingsSaveInterval = 5min;
#else
constexpr auto PlaylistSaveInterval = 30000;
constexpr auto SettingsSaveInterval = 300000;
#endif

namespace {
void registerTypes()
{
    qRegisterMetaType<Fooyin::Track>("Track");
    qRegisterMetaType<Fooyin::TrackList>("TrackList");
    qRegisterMetaType<Fooyin::TrackIds>("TrackIds");
    qRegisterMetaType<Fooyin::OutputCreator>("OutputCreator");
    qRegisterMetaType<Fooyin::LibraryInfo>("LibraryInfo");
    qRegisterMetaType<Fooyin::LibraryInfoMap>("LibraryInfoMap");
}
} // namespace

namespace Fooyin {
class ApplicationPrivate
{
public:
    explicit ApplicationPrivate(Application* self_);

    void registerPlaylistParsers();
    void registerTagParsers();
    void registerDecoders();

    void loadPlugins();

    void startSaveTimer();

    void savePlaybackState() const;
    void loadPlaybackState() const;

    Application* m_self;

    SettingsManager* m_settings;
    CoreSettings m_coreSettings;
    Translations m_translations;
    Database* m_database;
    std::shared_ptr<TagLoader> m_tagLoader;
    std::shared_ptr<DecoderProvider> m_decoderProvider;
    PlayerController* m_playerController;
    EngineHandler m_engine;
    LibraryManager* m_libraryManager;
    std::shared_ptr<PlaylistLoader> m_playlistLoader;
    UnifiedMusicLibrary* m_library;
    PlaylistHandler* m_playlistHandler;
    SortingRegistry* m_sortingRegistry;

    PluginManager m_pluginManager;
    CorePluginContext m_corePluginContext;

    QBasicTimer m_playlistSaveTimer;
    QBasicTimer m_settingsSaveTimer;
};

ApplicationPrivate::ApplicationPrivate(Application* self_)
    : m_self{self_}
    , m_settings{new SettingsManager(Core::settingsPath(), m_self)}
    , m_coreSettings{m_settings}
    , m_translations{m_settings}
    , m_database{new Database(m_self)}
    , m_tagLoader{std::make_shared<TagLoader>()}
    , m_decoderProvider{std::make_shared<DecoderProvider>()}
    , m_playerController{new PlayerController(m_settings, m_self)}
    , m_engine{m_decoderProvider, m_playerController, m_settings}
    , m_libraryManager{new LibraryManager(m_database->connectionPool(), m_settings, m_self)}
    , m_playlistLoader{std::make_shared<PlaylistLoader>()}
    , m_library{new UnifiedMusicLibrary(m_libraryManager, m_database->connectionPool(), m_playlistLoader, m_tagLoader,
                                        m_settings, m_self)}
    , m_playlistHandler{new PlaylistHandler(m_database->connectionPool(), m_tagLoader, m_playerController, m_settings,
                                            m_self)}
    , m_sortingRegistry{new SortingRegistry(m_settings, m_self)}
    , m_pluginManager{m_settings}
    , m_corePluginContext{&m_pluginManager, &m_engine,         m_playerController, m_libraryManager,
                          m_library,        m_playlistHandler, m_settings,         m_playlistLoader,
                          m_tagLoader,      m_decoderProvider, m_sortingRegistry}
{
    registerTypes();
    registerDecoders();
    registerTagParsers();
    registerPlaylistParsers();
    loadPlugins();

    m_settingsSaveTimer.start(SettingsSaveInterval, m_self);
}

void ApplicationPrivate::registerPlaylistParsers()
{
    m_playlistLoader->addParser(std::make_unique<CueParser>(m_tagLoader));
    m_playlistLoader->addParser(std::make_unique<M3uParser>(m_tagLoader));
}

void ApplicationPrivate::registerTagParsers()
{
    m_tagLoader->addParser(QStringLiteral("TagLib"), std::make_unique<TagLibParser>());
    m_tagLoader->addParser(QStringLiteral("FFmpeg"), std::make_unique<FFmpegParser>());
}

void ApplicationPrivate::registerDecoders()
{
    m_decoderProvider->addDecoder(QStringLiteral("FFmpeg"), FFmpegDecoder::extensions(),
                                  []() { return std::make_unique<FFmpegDecoder>(); });
}

void ApplicationPrivate::loadPlugins()
{
    const QStringList pluginPaths{Core::pluginPaths()};
    m_pluginManager.findPlugins(pluginPaths);
    m_pluginManager.loadPlugins();

    m_pluginManager.initialisePlugins<CorePlugin>(
        [this](CorePlugin* plugin) { plugin->initialise(m_corePluginContext); });

    m_pluginManager.initialisePlugins<OutputPlugin>(
        [this](OutputPlugin* plugin) { m_engine.addOutput(plugin->name(), plugin->creator()); });

    m_pluginManager.initialisePlugins<TagParserPlugin>(
        [this](TagParserPlugin* plugin) { m_tagLoader->addParser(plugin->parserName(), plugin->tagParser()); });

    m_pluginManager.initialisePlugins<DecoderPlugin>([this](DecoderPlugin* plugin) {
        m_decoderProvider->addDecoder(plugin->decoderName(), plugin->supportedExtensions(), plugin->decoderCreator());
    });
}

void ApplicationPrivate::startSaveTimer()
{
    m_playlistSaveTimer.start(PlaylistSaveInterval, m_self);
}

void ApplicationPrivate::savePlaybackState() const
{
    if(m_settings->value<Settings::Core::Internal::SavePlaybackState>()) {
        const auto lastPos = static_cast<quint64>(m_playerController->currentPosition());

        m_settings->fileSet(QString::fromLatin1(LastPlaybackPosition), lastPos);
        m_settings->fileSet(QString::fromLatin1(LastPlaybackState), static_cast<int>(m_playerController->playState()));
    }
    else {
        m_settings->fileRemove(QString::fromLatin1(LastPlaybackPosition));
        m_settings->fileRemove(QString::fromLatin1(LastPlaybackState));
    }
}

void ApplicationPrivate::loadPlaybackState() const
{
    if(!m_settings->value<Settings::Core::Internal::SavePlaybackState>()) {
        return;
    }

    const auto lastPos = m_settings->fileValue(QString::fromLatin1(LastPlaybackPosition)).value<uint64_t>();
    const auto state   = m_settings->fileValue(QString::fromLatin1(LastPlaybackState)).value<PlayState>();

    switch(state) {
        case PlayState::Paused:
            m_playerController->pause();
            break;
        case PlayState::Playing:
            m_playerController->play();
            break;
        case PlayState::Stopped:
            break;
    }

    m_playerController->seek(lastPos);
}

Application::Application(QObject* parent)
    : QObject{parent}
    , p{std::make_unique<ApplicationPrivate>(this)}
{
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::playlistTracksAdded, this,
                     [this]() { p->startSaveTimer(); });
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::playlistTracksChanged, this,
                     [this]() { p->startSaveTimer(); });
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::playlistTracksRemoved, this,
                     [this]() { p->startSaveTimer(); });
    QObject::connect(p->m_playlistHandler, &PlaylistHandler::playlistsPopulated, this,
                     [this]() { p->loadPlaybackState(); });

    QObject::connect(p->m_playerController, &PlayerController::trackPlayed, p->m_library,
                     &UnifiedMusicLibrary::trackWasPlayed);
    QObject::connect(p->m_library, &MusicLibrary::tracksLoaded, p->m_playlistHandler,
                     &PlaylistHandler::populatePlaylists);
    QObject::connect(p->m_libraryManager, &LibraryManager::libraryAboutToBeRemoved, p->m_playlistHandler,
                     &PlaylistHandler::savePlaylists);
    QObject::connect(p->m_library, &MusicLibrary::tracksUpdated, p->m_playlistHandler,
                     [this](const TrackList& tracks) { p->m_playlistHandler->tracksUpdated(tracks); });
    QObject::connect(p->m_library, &MusicLibrary::tracksPlayed, p->m_playlistHandler,
                     [this](const TrackList& tracks) { p->m_playlistHandler->tracksPlayed(tracks); });
    QObject::connect(&p->m_engine, &EngineHandler::trackAboutToFinish, p->m_playlistHandler,
                     &PlaylistHandler::trackAboutToFinish);
    QObject::connect(&p->m_engine, &EngineController::trackStatusChanged, this, [this](TrackStatus status) {
        if(status == TrackStatus::InvalidTrack) {
            p->m_playerController->pause();
        }
    });

    p->m_library->loadAllTracks();
    p->m_engine.setup();
}

Application::~Application() = default;

CorePluginContext Application::context() const
{
    return p->m_corePluginContext;
}

void Application::timerEvent(QTimerEvent* event)
{
    if(event->timerId() == p->m_playlistSaveTimer.timerId()) {
        p->m_playlistSaveTimer.stop();
        p->m_playlistHandler->savePlaylists();
    }
    else if(event->timerId() == p->m_settingsSaveTimer.timerId()) {
        if(p->m_settings->settingsHaveChanged()) {
            p->m_settings->storeSettings();
        }
    }

    QObject::timerEvent(event);
}

void Application::shutdown()
{
    p->savePlaybackState();
    p->m_playlistHandler->savePlaylists();
    p->m_coreSettings.shutdown();
    p->m_pluginManager.shutdown();
    p->m_settings->storeSettings();
    p->m_library->cleanupTracks();
}

void Application::quit()
{
    QMetaObject::invokeMethod(QCoreApplication::instance(), []() { QCoreApplication::quit(); }, Qt::QueuedConnection);
}

void Application::restart()
{
    QMetaObject::invokeMethod(
        QCoreApplication::instance(),
        []() {
            const QString appPath = QCoreApplication::applicationFilePath();
            QCoreApplication::quit();
            QProcess::startDetached(appPath, {QStringLiteral("-s")});
        },
        Qt::QueuedConnection);
}
} // namespace Fooyin

#include "moc_application.cpp"
