// SPDX-License-Identifier: GPL-2.0-only
#include "AdvancedSearchDialog.h"

#include <QtWidgets/qboxlayout.h>
#include <QtWidgets/qgridlayout.h>
#include <QtWidgets/qlabel.h>
#include <QtWidgets/qpushbutton.h>

AdvancedSearchDialog::AdvancedSearchDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Search");

    _artist = new QLineEdit(this);
    _album = new QLineEdit(this);
    _genre = new QLineEdit(this);
    _title = new QLineEdit(this);
    _comment = new QLineEdit(this);

    _check_case_sensitive = new QCheckBox("Case sensitive", this);
    _check_regex = new QCheckBox("Regular expressions", this);

    QPushButton* search_button = new QPushButton("Search", this);
    QPushButton* close_button = new QPushButton("Close", this);

    QGridLayout* options_layout = new QGridLayout();
    options_layout->addWidget(new QLabel("Artist", this), 0, 0);
    options_layout->addWidget(_artist, 0, 1);
    options_layout->addWidget(new QLabel("Album", this), 1, 0);
    options_layout->addWidget(_album, 1, 1);
    options_layout->addWidget(new QLabel("Genre", this), 2, 0);
    options_layout->addWidget(_genre, 2, 1);
    options_layout->addWidget(new QLabel("Title", this), 3, 0);
    options_layout->addWidget(_title, 3, 1);
    options_layout->addWidget(new QLabel("Comment", this), 4, 0);
    options_layout->addWidget(_comment, 4, 1);
    options_layout->addWidget(_check_case_sensitive, 5, 0, 1, 2);
    options_layout->addWidget(_check_regex, 6, 0, 1, 2);
    options_layout->addItem(new QSpacerItem(0, 0), 7, 0);
    options_layout->setRowStretch(7, 1);

    QVBoxLayout* buttons_layout = new QVBoxLayout();
    buttons_layout->addWidget(search_button);
    buttons_layout->addWidget(close_button);
    buttons_layout->addStretch();

    QHBoxLayout* top_layout = new QHBoxLayout(this);
    top_layout->addLayout(options_layout);
    top_layout->addLayout(buttons_layout);

    connect(search_button, &QPushButton::clicked, this, &AdvancedSearchDialog::emitSearchRequested);
    connect(close_button, &QPushButton::clicked, this, &AdvancedSearchDialog::close);

    show();

    _artist->setFocus();
}

void AdvancedSearchDialog::emitSearchRequested()
{
    AudioLibraryViewAdvancedSearch::Query query;
    query.artist = _artist->text();
    query.album = _album->text();
    query.genre = _genre->text();
    query.title = _title->text();
    query.comment = _comment->text();

    if (query.artist.isEmpty() &&
        query.album.isEmpty() &&
        query.genre.isEmpty() &&
        query.title.isEmpty() &&
        query.comment.isEmpty())
    {
        // don't start an empty search
        return;
    }

    query.case_sensitive = _check_case_sensitive->isChecked() ? Qt::CaseSensitive : Qt::CaseInsensitive;
    query.use_regex = _check_regex->isChecked();

    searchRequested(query);
}