// SPDX-License-Identifier: GPL-2.0-only
#include "MainWindow.h"

#include <QtWidgets/qapplication.h>
#include "project_version.h"

int main(int argc, char** argv)
{
    QApplication app(argc, argv);
    app.setApplicationName(APPLICATION_NAME);

    Settings settings;
    MainWindow main(settings);

    return app.exec();
}