// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <atomic>
#include <mutex>
#include <thread>
#include <QtCore/qobject.h>
#include "AudioLibrary.h"

class SpinLock
{
public:
    void lock()
    {
        while (locked.test_and_set(std::memory_order_acquire))
            ;
    }

    void unlock()
    {
        locked.clear(std::memory_order_release);
    }

private:
    std::atomic_flag locked = ATOMIC_FLAG_INIT;
};

class ThreadSafeAudioLibrary
{
public:
    class LibraryAccessor
    {
    public:
        LibraryAccessor(ThreadSafeAudioLibrary& data);

        const AudioLibrary& getLibrary() const;
        AudioLibrary& getLibraryForUpdate() const;

    private:
        std::lock_guard<SpinLock> _lock;
        AudioLibrary& _library;
    };

    bool hasFinishedLoadingFromCache() const;
    void setFinishedLoadingFromCache();
    void saveToCache(const QString& cache_dir, const QString& cache_location);


private:
    SpinLock _library_spin_lock;
    AudioLibrary _library;
    std::atomic_bool _has_finished_loading_from_cache = ATOMIC_VAR_INIT(false);
};

class AudioFilesLoader : public QObject
{
    Q_OBJECT

public:
    AudioFilesLoader(ThreadSafeAudioLibrary& library);
    ~AudioFilesLoader();

    void startLoading(const QString& cache_location, const QStringList& audio_dir_paths);
    bool isLoading() const;

signals:
    void libraryCacheLoading();
    void libraryLoadProgressed(int files_loaded, int files_in_cache);
    void libraryLoadFinished(int files_loaded, int files_in_cache, float duration_sec);
    void coverLoadFinished();

private:
    void stopLoading();
    void loadFromCache(const QString& cache_location);
    void threadLoadAudioFiles(const QString& cache_location, const QStringList& audio_dir_paths);
    void threadDecodeCovers();

    ThreadSafeAudioLibrary& _library;

    std::thread _audio_file_loading_thread;
    std::thread _cover_decoding_thread;

    std::atomic_bool _thread_abort_flag = ATOMIC_VAR_INIT(false);

    // keeps the cover decoding thread alive
    std::atomic_bool _is_loading = ATOMIC_VAR_INIT(false);
};