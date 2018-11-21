// SPDX-License-Identifier: GPL-2.0-only
#pragma once

#include <QtWidgets/qcheckbox.h>
#include <QtWidgets/qdialog.h>
#include <QtWidgets/qlineedit.h>
#include "AudioLibraryView.h"

class AdvancedSearchDialog : public QDialog
{
    Q_OBJECT

public:
    AdvancedSearchDialog(QWidget* parent);

signals:
    void searchRequested(AudioLibraryViewAdvancedSearch::Query query);

private:
    void emitSearchRequested();

    QLineEdit* _artist = nullptr;
    QLineEdit* _album = nullptr;
    QLineEdit* _genre = nullptr;
    QLineEdit* _title = nullptr;
    QLineEdit* _comment = nullptr;

    QCheckBox* _check_case_sensitive = nullptr;
    QCheckBox* _check_regex = nullptr;
};