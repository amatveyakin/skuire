#ifndef _PANELPOPUP_H
#define _PANELPOPUP_H

#include <qwidget.h>
#include <qwidgetstack.h>
#include <qpixmap.h>
#include <kfileitem.h>
#include <qguardedptr.h>
#include <kio/previewjob.h>
#include <kurl.h>

class QButtonGroup;
class QLabel;
class QListViewItem;
class KFileTreeView;
class QToolButton;
class KrSqueezedTextLabel;
class KLineEdit;
class KComboBox;
class KImageFilePreview;

class PanelPopup: public QWidget {
   Q_OBJECT
   enum Parts { Tree, Preview, QuickPanel, Last=0xFF };
public:
   PanelPopup( QWidget *parent, bool left );
   ~PanelPopup();
	inline int currentPage() const { return stack->id(stack->visibleWidget()); }

public slots:
   void update(KURL url);
	
signals:
	void selection(const KURL &url);
	void hideMe();
   
protected slots:	
	void tabSelected(int id);
	void treeSelection(QListViewItem*);
	void slotDroppedOnTree(QWidget *widget, QDropEvent *e, KURL::List &lst, KURL &);
	
	void quickSelect();
	void quickSelect(const QString &);
        void quickSelectStore();

protected:
   QWidgetStack *stack;
	KImageFilePreview *viewer;
	KrSqueezedTextLabel *dataLine;
	QGuardedPtr<KIO::PreviewJob> pjob;
	KFileTreeView *tree;
	QToolButton *treeBtn, *previewBtn, *quickBtn;
	QButtonGroup *btns;
	KLineEdit *quickFilter;
	KComboBox *quickSelectCombo;
};

#endif // _PANELPOPUP_H
