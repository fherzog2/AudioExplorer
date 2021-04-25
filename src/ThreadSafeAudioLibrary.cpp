// SPDX-License-Identifier: GPL-2.0-only

#include "ThreadSafeAudioLibrary.h"

#include <QtCore/qsavefile.h>

namespace {
    template<class T, class V>
    class SetValueOnDestroy
    {
    public:
        SetValueOnDestroy(T& variable, V value)
            : _variable(variable)
            , _value(value)
        {}

        ~SetValueOnDestroy()
        {
            _variable = _value;
        }

        SetValueOnDestroy(const SetValueOnDestroy& other) = delete;
        SetValueOnDestroy& operator=(const SetValueOnDestroy& other) = delete;
        SetValueOnDestroy(SetValueOnDestroy&& other) = delete;
        SetValueOnDestroy& operator=(SetValueOnDestroy&& other) = delete;

    private:
        T& _variable;
        V _value;
    };

    template<class FUNC>
    void forEachFileInDirectory(const QString& dirpath, FUNC func)
    {
        std::vector<QString> queue;
        queue.push_back(dirpath);

        while (!queue.empty())
        {
            QString current_dir = queue.front();
            queue.erase(queue.begin());

            QDir dir(current_dir);

            QStringList subdirs = dir.entryList(QDir::AllDirs | QDir::NoDotAndDotDot);
            for (const QString& subdir : subdirs)
                queue.push_back(current_dir + "/" + subdir);

            QFileInfoList files = dir.entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
            for (const QFileInfo& file : files)
            {
                if (!func(file))
                    return;
            }
        }
    }
} // namespace

//=============================================================================

ThreadSafeAudioLibrary::LibraryAccessor::LibraryAccessor(ThreadSafeAudioLibrary& data)
    : _lock(data._library_spin_lock)
    , _library(data._library)
{
}

const AudioLibrary& ThreadSafeAudioLibrary::LibraryAccessor::getLibrary() const
{
    return _library;
}

AudioLibrary& ThreadSafeAudioLibrary::LibraryAccessor::getLibraryForUpdate() const
{
    return _library;
}

//=============================================================================

bool ThreadSafeAudioLibrary::hasFinishedLoadingFromCache() const
{
    return _has_finished_loading_from_cache;
}

void ThreadSafeAudioLibrary::setFinishedLoadingFromCache()
{
    _has_finished_loading_from_cache = true;
}

void ThreadSafeAudioLibrary::setCacheLocation(const QString& cache_location)
{
    _cache_location = cache_location;
}

QString ThreadSafeAudioLibrary::getCacheLocation() const
{
    return _cache_location;
}

void ThreadSafeAudioLibrary::saveToCache()
{
    if (!_has_finished_loading_from_cache)
        return; // don't save back a partially loaded library

    {
        ThreadSafeAudioLibrary::LibraryAccessor acc(*this);

        if (!acc.getLibrary().isModified())
            return; // no need to save if the library has not changed
    }

    {
        const QString cache_dir = QFileInfo(_cache_location).path();

        QDir dir(cache_dir);
        if (!dir.mkpath(cache_dir))
            return;
    }

    QSaveFile file(_cache_location);
    if (!file.open(QIODevice::WriteOnly))
        return;

    {
        QDataStream stream(&file);

        ThreadSafeAudioLibrary::LibraryAccessor acc(*this);

        acc.getLibrary().save(stream);
    }

    file.commit();
}

//=============================================================================

AudioFilesLoader::AudioFilesLoader(ThreadSafeAudioLibrary& library)
    : _library(library)
{
}

AudioFilesLoader::~AudioFilesLoader()
{
    stopLoading();
}

void AudioFilesLoader::startLoading(const QStringList& audio_dir_paths)
{
    const QString cache_location = _library.getCacheLocation();

    // stop existing threads before creating new ones

    stopLoading();

    // start

    _thread_abort_flag = false;
    _is_loading = true;

    _audio_file_loading_thread = std::thread([this, cache_location, audio_dir_paths](){
        threadLoadAudioFiles(cache_location, audio_dir_paths);
    });
}

bool AudioFilesLoader::isLoading() const
{
    return _is_loading;
}

void AudioFilesLoader::stopLoading()
{
    _thread_abort_flag = true;

    if (_audio_file_loading_thread.joinable())
        _audio_file_loading_thread.join();
}

void AudioFilesLoader::loadFromCache(const QString& cache_location)
{
    QFile file(cache_location);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QDataStream stream(&file);

    AudioLibrary::Loader loader;

    {
        ThreadSafeAudioLibrary::LibraryAccessor acc(_library);

        loader.init(acc.getLibraryForUpdate(), stream);
    }

    int album_counter = 0;

    while (loader.hasNextAlbum())
    {
        {
            ThreadSafeAudioLibrary::LibraryAccessor acc(_library);

            loader.loadNextAlbum(acc.getLibraryForUpdate());
        }

        ++album_counter;

        if ((album_counter % 10) == 0)
            libraryCacheLoading();
    }
}

void AudioFilesLoader::threadLoadAudioFiles(const QString& cache_location, const QStringList& audio_dir_paths)
{
    SetValueOnDestroy<std::atomic_bool, bool> reset_loading_flag(_is_loading, false);

    int files_loaded = 0;
    int files_in_cache = 0;
    auto start_time = std::chrono::system_clock::now();

    if (!_library.hasFinishedLoadingFromCache())
    {
        loadFromCache(cache_location);
        _library.setFinishedLoadingFromCache();
        libraryCacheLoading();
    }

    std::unordered_set<QString> visited_audio_files;

    for (const QString& dirpath : audio_dir_paths)
    {
        forEachFileInDirectory(dirpath, [this, &files_loaded, &files_in_cache, &visited_audio_files](const QFileInfo& file) {
            if (_thread_abort_flag)
                return false; // stop iteration

            const QString filepath = file.filePath();
            const QDateTime last_modified = file.lastModified();

            {
                ThreadSafeAudioLibrary::LibraryAccessor acc(_library);

                if (const AudioLibraryTrack * track = acc.getLibrary().findTrack(filepath))
                    if (track->getLastModified() == last_modified)
                    {
                        ++files_in_cache;
                        visited_audio_files.insert(filepath);
                        libraryLoadProgressed(files_loaded, files_in_cache);
                        return true; // nothing to do
                    }
            }

            TrackInfo track_info;
            if (readTrackInfo(filepath, track_info))
            {
                {
                    ThreadSafeAudioLibrary::LibraryAccessor acc(_library);

                    acc.getLibraryForUpdate().addTrack(filepath, last_modified, file.size(), track_info);
                }

                ++files_loaded;
                visited_audio_files.insert(filepath);
                libraryLoadProgressed(files_loaded, files_in_cache);
            }
            return true;
            });
    }

    if (!_thread_abort_flag)
    {
        ThreadSafeAudioLibrary::LibraryAccessor acc(_library);

        acc.getLibraryForUpdate().removeTracksExcept(visited_audio_files);
    }

    auto end_time = std::chrono::system_clock::now();
    auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

    libraryLoadFinished(files_loaded, files_in_cache, float(millis.count()) / 1000.0);
}