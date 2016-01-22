#include "viewplaylists.h"

#include <library/jumptowidget.h>
#include <settingsprivate.h>
#include <libraryorderdialog.h>

ViewPlaylists::ViewPlaylists(MediaPlayer *mediaPlayer)
	: AbstractViewPlaylists(mediaPlayer)
	, _searchDialog(new SearchDialog(this))
	, _playbackModeWidgetFactory(nullptr)
{
	this->setupUi(this);
	paintableWidget->setFrameBorder(false, false, true, false);
	seekSlider->setMediaPlayer(_mediaPlayer);
	tabPlaylists->init(_mediaPlayer);
	widgetSearchBar->setFrameBorder(false, false, true, false);

	Settings *settings = Settings::instance();
	volumeSlider->setValue(settings->volume() * 100);

	// Special behaviour for media buttons
	mediaButtons << skipBackwardButton << seekBackwardButton << playButton << stopButton
				 << seekForwardButton << skipForwardButton << playbackModeButton;

	// Buttons
	SettingsPrivate *settingsPrivate = SettingsPrivate::instance();
	for (MediaButton *b : mediaButtons) {
		b->setMediaPlayer(_mediaPlayer);

		if (!b) {
			continue;
		}
		b->setSize(settingsPrivate->buttonsSize());
		b->setMediaPlayer(_mediaPlayer);
		if (settingsPrivate->isButtonThemeCustomized()) {
			b->setIcon(QIcon(settingsPrivate->customIcon(b->objectName())));
		} else {
			b->setIconFromTheme(settings->theme());
		}
		b->setVisible(settingsPrivate->isMediaButtonVisible(b->objectName()));
	}

	// Instantiate dialogs
	_playbackModeWidgetFactory = new PlaybackModeWidgetFactory(this, playbackModeButton, tabPlaylists);

	searchBar->setFont(settingsPrivate->font(SettingsPrivate::FF_Library));

	bool isEmpty = settingsPrivate->musicLocations().isEmpty();
	if (isEmpty) {
		widgetSearchBar->hide();
		changeHierarchyButton->hide();
		libraryHeader->hide();
		library->hide();
		//QuickStart *quickStart = new QuickStart(this);
		//quickStart->searchMultimediaFiles();
	}

	leftTabs->setCurrentIndex(settingsPrivate->value("leftTabsIndex").toInt());

	// Core
	connect(_mediaPlayer, &MediaPlayer::stateChanged, this, &ViewPlaylists::mediaPlayerStateHasChanged);



	// Main Splitter
	connect(splitter, &QSplitter::splitterMoved, _searchDialog, &SearchDialog::moveSearchDialog);

	connect(searchBar, &LibraryFilterLineEdit::aboutToStartSearch, library->model()->proxy(), &LibraryFilterProxyModel::findMusic);
	connect(settingsPrivate, &SettingsPrivate::librarySearchModeHasChanged, this, [=]() {
		QString text;
		searchBar->setText(text);
		library->model()->proxy()->findMusic(text);
	});

	// Playback
	connect(tabPlaylists, &TabPlaylist::updatePlaybackModeButton, _playbackModeWidgetFactory, &PlaybackModeWidgetFactory::update);

	// Media buttons
	connect(skipBackwardButton, &QAbstractButton::clicked, _mediaPlayer, &MediaPlayer::skipBackward);
	connect(seekBackwardButton, &QAbstractButton::clicked, _mediaPlayer, &MediaPlayer::seekBackward);
	connect(playButton, &QAbstractButton::clicked, _mediaPlayer, &MediaPlayer::togglePlayback);
	connect(stopButton, &QAbstractButton::clicked, _mediaPlayer, &MediaPlayer::stop);
	connect(seekForwardButton, &QAbstractButton::clicked, _mediaPlayer, &MediaPlayer::seekForward);
	connect(skipForwardButton, &QAbstractButton::clicked, _mediaPlayer, &MediaPlayer::skipForward);
	connect(playbackModeButton, &MediaButton::mediaButtonChanged, _playbackModeWidgetFactory, &PlaybackModeWidgetFactory::update);

	connect(filesystem, &FileSystemTreeView::folderChanged, addressBar, &AddressBar::init);
	connect(addressBar, &AddressBar::aboutToChangePath, filesystem, &FileSystemTreeView::reloadWithNewPath);
	addressBar->init(settingsPrivate->defaultLocationFileExplorer());

	// Playback modes
	connect(playbackModeButton, &QPushButton::clicked, _playbackModeWidgetFactory, &PlaybackModeWidgetFactory::togglePlaybackModes);

	connect(libraryHeader, &LibraryHeader::aboutToChangeSortOrder, library, &LibraryTreeView::changeSortOrder);

	// Factorize code with lambda slot connected to replicated signal
	auto reloadLibrary = [this]() {
		searchBar->setText(QString());
		_searchDialog->clear();
		SqlDatabase::instance()->load();
		this->update();
	};

	connect(libraryHeader, &LibraryHeader::aboutToChangeHierarchyOrder, reloadLibrary);
	connect(changeHierarchyButton, &QPushButton::toggled, this, [=]() {
		LibraryOrderDialog *libraryOrderDialog = new LibraryOrderDialog(this);
		libraryOrderDialog->move(libraryOrderDialog->mapFromGlobal(QCursor::pos()));
		libraryOrderDialog->show();
		connect(libraryOrderDialog, &LibraryOrderDialog::aboutToChangeHierarchyOrder, reloadLibrary);
	});

	for (TreeView *tab : this->findChildren<TreeView*>()) {
		connect(tab, &TreeView::aboutToInsertToPlaylist, tabPlaylists, &TabPlaylist::insertItemsToPlaylist);
		connect(tab, &TreeView::sendToTagEditor, this, [=](const QModelIndexList , const QList<QUrl> &tracks) {
			/// FIXME
			//this->showTagEditor();
			//tagEditor->addItemsToEditor(tracks);
		});
	}

	// Send one folder to the music locations
	connect(filesystem, &FileSystemTreeView::aboutToAddMusicLocations, settingsPrivate, &SettingsPrivate::addMusicLocations);

	// Send music to the tag editor
	/// FIXME
	//connect(tagEditor, &TagEditor::aboutToCloseTagEditor, this, &MainWindow::showTabPlaylists);
	connect(tabPlaylists, &TabPlaylist::aboutToSendToTagEditor, [=](const QList<QUrl> &tracks) {
		/// FIXME
		//this->showTagEditor();
		//tagEditor->addItemsToEditor(tracks);
	});

	// Sliders
	connect(_mediaPlayer, &MediaPlayer::positionChanged, [=] (qint64 pos, qint64 duration) {
		if (duration > 0) {
			seekSlider->setValue(1000 * pos / duration);
			timeLabel->setTime(pos, duration);
		}
	});

	// Volume bar
	connect(volumeSlider, &QSlider::valueChanged, this, [=](int value) {
		_mediaPlayer->setVolume((qreal)value / 100.0);
	});

	connect(qApp, &QApplication::aboutToQuit, this, [=] {
		settingsPrivate->setValue("leftTabsIndex", leftTabs->currentIndex());
		settingsPrivate->setLastActivePlaylistGeometry(tabPlaylists->currentPlayList()->horizontalHeader()->saveState());
		settingsPrivate->sync();
	});

	library->createConnectionsToDB();
	connect(SqlDatabase::instance(), &SqlDatabase::aboutToLoad, libraryHeader, &LibraryHeader::resetSortOrder);

	// Filter the library when user is typing some text to find artist, album or tracks
	connect(searchBar, &LibraryFilterLineEdit::aboutToStartSearch, this, [=](const QString &text) {
		if (settingsPrivate->isExtendedSearchVisible()) {
			if (text.isEmpty()) {
				_searchDialog->clear();
			} else {
				_searchDialog->setSearchExpression(text);
				_searchDialog->moveSearchDialog(0, 0);
				_searchDialog->show();
				_searchDialog->raise();
			}
		}
	});
}

int ViewPlaylists::selectedTracksInCurrentPlaylist() const
{
	return tabPlaylists->currentPlayList()->selectionModel()->selectedRows().count();
}

void ViewPlaylists::moveEvent(QMoveEvent *event)
{
	/// FIXME
	_playbackModeWidgetFactory->move();
	AbstractView::moveEvent(event);
}

void ViewPlaylists::addPlaylist()
{
	tabPlaylists->addPlaylist();
}

void ViewPlaylists::removeCurrentPlaylist()
{
	tabPlaylists->removeCurrentPlaylist();
}

void ViewPlaylists::volumeSliderDecrease()
{
	volumeSlider->setValue(volumeSlider->value() - 5);
}

void ViewPlaylists::volumeSliderIncrease()
{
	volumeSlider->setValue(volumeSlider->value() + 5);
}

void ViewPlaylists::mediaPlayerStateHasChanged(QMediaPlayer::State state)
{
	if (state == QMediaPlayer::PlayingState) {
		QString iconPath;
		if (SettingsPrivate::instance()->hasCustomIcon("pauseButton")) {
			iconPath = SettingsPrivate::instance()->customIcon("pauseButton");
		} else {
			iconPath = ":/player/" + Settings::instance()->theme() + "/pause";
		}
		playButton->setIcon(QIcon(iconPath));
		seekSlider->setEnabled(true);
	} else {
		playButton->setIcon(QIcon(":/player/" + Settings::instance()->theme() + "/play"));
		seekSlider->setDisabled(state == QMediaPlayer::StoppedState);
		if (state == QMediaPlayer::StoppedState) {
			seekSlider->setValue(0);
			timeLabel->setTime(0, 0);
		}
	}
	// Remove bold font when player has stopped
	tabPlaylists->currentPlayList()->viewport()->update();
	seekSlider->update();
}