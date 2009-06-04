/***************************************************************************
                          lister.h  -  description
                             -------------------
    copyright            : (C) 2009 + by Csaba Karai
    e-mail               : krusader@users.sourceforge.net
    web site             : http://krusader.sourceforge.net
 ---------------------------------------------------------------------------
  Description
 ***************************************************************************

  A

     db   dD d8888b. db    db .d8888.  .d8b.  d8888b. d88888b d8888b.
     88 ,8P' 88  `8D 88    88 88'  YP d8' `8b 88  `8D 88'     88  `8D
     88,8P   88oobY' 88    88 `8bo.   88ooo88 88   88 88ooooo 88oobY'
     88`8b   88`8b   88    88   `Y8b. 88~~~88 88   88 88~~~~~ 88`8b
     88 `88. 88 `88. 88b  d88 db   8D 88   88 88  .8D 88.     88 `88.
     YP   YD 88   YD ~Y8888P' `8888Y' YP   YP Y8888D' Y88888P 88   YD

                                                     H e a d e r    F i l e

 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#ifndef __LISTER_H__
#define __LISTER_H__

#include <kparts/part.h>
#include <qwidget.h>
#include <QtGui/QTextEdit>
#include <QList>

#define  LISTER_CACHE_FACTOR 3
#define  SLIDER_MAX          10000
#define  MAX_CHAR_LENGTH     4

class QTextEdit;
class Lister;

class ListerTextArea : public QTextEdit
{
Q_OBJECT

public:
  ListerTextArea( Lister *lister, QWidget *parent );
  void           reset();
  void           calculateText();
  void           redrawTextArea();

  qint64         textToFilePosition( int x, int y, bool &isfirst );
  void           fileToTextPosition( qint64 p, bool isfirst, int &x, int &y );

  QTextCodec   * codec();

protected:
  virtual void   resizeEvent ( QResizeEvent * event );
  virtual void   keyPressEvent( QKeyEvent * e );

  QStringList    readLines( qint64 filePos, qint64 &endPos, int lines, QList<qint64> * locs = 0 );
  void           setUpScrollBar();
  void           getCursorPosition( int &x, int &y );
  void           setCursorPosition( int x, int y, int anchorX = -1, int anchorY = -1);
  qint64         getCursorPosition( bool &isfirst );
  void           setCursorPosition( qint64 p, bool isfirst );
  void           ensureVisibleCursor();

protected slots:
  void           slotActionTriggered( int action );
  void           slotCursorPositionChanged();

protected:
  Lister        *_lister;

  qint64         _screenStartPos;
  qint64         _screenEndPos;
  qint64         _averagePageSize;

  qint64         _lastPageStartPos;

  int            _sizeX;
  int            _sizeY;
  int            _pageSize;

  int            _tabWidth;

  bool           _sizeChanged;

  QStringList    _rowContent;
  QList<qint64>  _rowStarts;

  qint64         _cursorPos;
  bool           _cursorAtFirstColumn;

  qint64         _cursorAnchorPos;

  int            _skippedLines;

  bool           _inSliderOp;
  bool           _inCursorUpdate;
};

class Lister : public KParts::ReadOnlyPart
{
public:
  Lister( QWidget *parent );
  ~Lister();

  QScrollBar     *scrollBar() { return _scrollBar; }

  inline qint64  fileSize() { return _fileSize; }
  char *         cacheRef( qint64 filePos, int &size );

protected:
  virtual bool    openFile();
  qint64          getFileSize();

  ListerTextArea *_textArea;
  QScrollBar     *_scrollBar;

  QString         _filePath;
  qint64          _fileSize;

  char           *_cache;
  int             _cacheSize;
  qint64          _cachePos;
};

#endif // __LISTER_H__
