// SPDX-License-Identifier: GPL-2.0-only
#include "MainWindow.h"

#include <QtCore/qtranslator.h>
#include <QtCore/qstandardpaths.h>
#include <QtCore/qlibraryinfo.h>
#include <QtWidgets/qapplication.h>
#include <QtWidgets/qmessagebox.h>
#include <QtWidgets/QStyleFactory>
#include <QtGui/QStyleHints>
#include "project_version.h"

class TranslationManager
{
public:
    TranslationManager(QApplication* app);

    QString getLanguage() const;
    void setLanguage(const QString& lang);

    QString getSupportedSystemLanguage() const;

private:
    void setLanguageInternal(const QString& lang);

    std::unordered_map<QString, std::vector<QString>> _translation_files;

    QApplication* _app = nullptr;
    QString _current_lang;
    std::vector<QTranslator*> _translators;
};

TranslationManager::TranslationManager(QApplication* app)
    : _app(app)
{
    _translation_files["de"] = {
        ":/res/AudioExplorer_de.qm"
    };

    _translation_files["en"] = {
        // no file needed, because the default texts are already in english
    };
}

QString TranslationManager::getLanguage() const
{
    return _current_lang;
}

void TranslationManager::setLanguage(const QString& lang)
{
    setLanguageInternal(lang.isEmpty() ? getSupportedSystemLanguage() : lang);
}

QString TranslationManager::getSupportedSystemLanguage() const
{
    // try each UI language

    for (const QString& ui_lang : QLocale().uiLanguages())
    {
        if (ui_lang.size() >= 2)
        {
            const QString candidate = ui_lang.left(2);

            if (_translation_files.contains(candidate))
                return candidate;
        }
    }

    // fallback to english if the system languages are not supported
    return "en";
}

void TranslationManager::setLanguageInternal(const QString& lang)
{
    auto it = _translation_files.find(lang);

    if (_current_lang == it->first)
        return;

    _current_lang = it->first;

    for (auto translator : _translators)
        translator->deleteLater();
    _translators.clear();

    for (const auto& translation_file : it->second)
    {
        auto translator = new QTranslator(_app);
        _translators.push_back(translator);

        // the translation file must stay in memory as long as the translator holds it
        // this is no problem with res::data as it is part of the application itself
        if (translator->load(translation_file))
            _app->installTranslator(translator);
    }

    // load Qt's own translation file

    const QString languages_path = QLibraryInfo::path(QLibraryInfo::TranslationsPath);

    auto translator = new QTranslator(_app);
    _translators.push_back(translator);

    if (translator->load(QString("qt_%1").arg(lang), languages_path))
        _app->installTranslator(translator);
}

class MainWindowCreator : public QObject
{
public:
    MainWindowCreator(Settings& settings, ThreadSafeAudioLibrary& library, AudioFilesLoader& audio_files_loader, TranslationManager& translation_manager);

    void create();

private:
    void checkLanguageChangedSlot();

    Settings& _settings;
    ThreadSafeAudioLibrary& _library;
    AudioFilesLoader& _audio_files_loader;
    TranslationManager& _translation_manager;
    std::unique_ptr<MainWindow> _main;
};

MainWindowCreator::MainWindowCreator(Settings& settings, ThreadSafeAudioLibrary& library, AudioFilesLoader& audio_files_loader, TranslationManager& translation_manager)
    : _settings(settings)
    , _library(library)
    , _audio_files_loader(audio_files_loader)
    , _translation_manager(translation_manager)
{}

void MainWindowCreator::create()
{
    _main = std::make_unique<MainWindow>(_settings, _library, _audio_files_loader);
    connect(_main.get(), &MainWindow::checkLanguageChanged, this, &MainWindowCreator::checkLanguageChangedSlot);
}

void MainWindowCreator::checkLanguageChangedSlot()
{
    const QString new_language = _settings.language.getValue();
    const QString new_language_resolved = new_language.isEmpty() ? _translation_manager.getSupportedSystemLanguage() : new_language;

    if (new_language_resolved == _translation_manager.getLanguage())
        return; // nothing to do

    if (QMessageBox::question(_main.get(),
                              QObject::tr("Restart program?"),
                              QObject::tr("Do you want to restart the program now to change the language?")) == QMessageBox::StandardButton::No)
        return;

    _translation_manager.setLanguage(new_language);

    // Recreate the main window.
    // The old window is safely destroyed with deleteLater,
    // and as soon as it is destroyed, a new main window takes its place.
    // The old main window has to be destroyed before the new one can be created,
    // because it is writing back its settings and cache-data during destruction.

    connect(_main.get(), &QObject::destroyed, this, [this]() {
        create();
    });

    _main.release()->deleteLater();
}

void enableDarkModeSupport()
{
#ifdef WIN32
#if QT_VERSION >= QT_VERSION_CHECK(6,5,0)
    QString original_style_name;
    if (auto original_style = qApp->style())
        original_style_name = original_style->name();

    auto adjust_style_to_color_scheme = [original_style_name]() {
        QString name = qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark ? "fusion" : original_style_name;
        if (auto style = QStyleFactory::create(name))
            qApp->setStyle(style);
    };

    adjust_style_to_color_scheme();

    QObject::connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, qApp, adjust_style_to_color_scheme);
#endif
#endif
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(APPLICATION_NAME);

    enableDarkModeSupport();

    Settings settings;

    ThreadSafeAudioLibrary library;
    const QString cache_dir = QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    const QString filepath = cache_dir + "/AudioLibrary";
    library.setCacheLocation(filepath);

    AudioFilesLoader audio_files_loader(library);
    audio_files_loader.startLoading(settings.audio_dir_paths.getValue());

    TranslationManager translation_manager(&app);
    translation_manager.setLanguage(settings.language.getValue());

    MainWindowCreator main_creator(settings, library, audio_files_loader, translation_manager);
    main_creator.create();

    return app.exec();
}