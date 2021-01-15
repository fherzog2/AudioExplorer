// SPDX-License-Identifier: GPL-2.0-only
#include "MainWindow.h"

#include <QtCore/qtranslator.h>
#include <QtWidgets/qapplication.h>
#include "project_version.h"
#include "compiled_resources.h"

class TranslationManager
{
public:
    TranslationManager(QApplication* app);

    void setLanguage(const QString& lang);

private:
    void setLanguageInternal(const QString& lang);

    std::unordered_map<QString, std::vector<res::data>> _translation_files;

    QApplication* _app = nullptr;
    QString _current_lang;
    std::vector<QTranslator*> _translators;
};

TranslationManager::TranslationManager(QApplication* app)
    : _app(app)
{
    _translation_files["de"] = {
        res::QT_DE_QM(),
        res::QTBASE_DE_QM(),
        res::AUDIOEXPLORER_DE_QM()
    };

    _translation_files["en"] = {
        res::QT_EN_QM(),
        res::QTBASE_EN_QM()
    };
}

void TranslationManager::setLanguage(const QString& lang)
{
    if (lang.isEmpty())
    {
        // use OS language

        const QStringList ui_languages = QLocale().uiLanguages();
        if (!ui_languages.isEmpty())
        {
            const QString ui_lang = ui_languages.front();
            if (ui_lang.size() >= 2)
            {
                setLanguageInternal(ui_lang.left(2));
                return;
            }
        }
    }

    setLanguageInternal(lang);
}

void TranslationManager::setLanguageInternal(const QString& lang)
{
    auto it = _translation_files.find(lang);
    if (it == _translation_files.end())
    {
        // fallback to english if the language is not supported
        it = _translation_files.find("en");
    }

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
        translator->load(reinterpret_cast<const unsigned char*>(translation_file.ptr), static_cast<int>(translation_file.size));
        _app->installTranslator(translator);
    }
}

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(APPLICATION_NAME);

    Settings settings;

    TranslationManager translation_manager(&app);
    translation_manager.setLanguage(settings.language.getValue());

    MainWindow main(settings);

    return app.exec();
}