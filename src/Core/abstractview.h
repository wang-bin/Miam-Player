#ifndef ABSTRACTVIEW_H
#define ABSTRACTVIEW_H

#include "miamcore_global.h"
#include "mediaplayer.h"
#include "mediaplayercontrol.h"
#include "settings.h"
#include <QDir>

class MusicSearchEngine;

/**
 * \brief		The AbstractView class is the base class for all views in Miam-Player.
 * \details		Every view in the player should inherit from this hierarchy in order to provide minimal functionalities
 *				to have a ready-to-work player. Usually, a view in a media player has a seekbar, a volume slider and some specific areas.
 *				But it can be completely different, based on the general properties each Media Control View offers.
 * \author      Matthieu Bachelier
 * \copyright   GNU General Public License v3
 */
class MIAMCORE_LIBRARY AbstractView : public QWidget
{
	Q_OBJECT
protected:
	MediaPlayerControl *_mediaPlayerControl;

public:
	AbstractView(MediaPlayerControl *mediaPlayerControl, QWidget *parent = nullptr)
		: QWidget(parent)
		, _mediaPlayerControl(mediaPlayerControl) {}

	virtual ~AbstractView() {}

	virtual QPair<QString, QObjectList> extensionPoints() const { return qMakePair(QString(), QObjectList()); }

	inline MediaPlayerControl* mediaPlayerControl() const { return _mediaPlayerControl; }

	virtual void setMusicSearchEngine(MusicSearchEngine *) {}

	virtual bool viewProperty(Settings::ViewProperty) const { return false; }

public slots:
	virtual void initFileExplorer(const QDir &) {}

	virtual void setViewProperty(Settings::ViewProperty vp, QVariant value) = 0;

	virtual void volumeSliderIncrease() {}

	virtual void volumeSliderDecrease() {}
};

#endif // ABSTRACTVIEW_H
