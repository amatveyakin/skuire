#ifndef __krvfsmodel__
#define __krvfsmodel__

#include <QAbstractListModel>
#include <QVector>
#include "krview.h"

class vfs;
class vfile;
class KrViewProperties;
class KrView;

class KrVfsModel: public QAbstractListModel {
	Q_OBJECT
	
public:
	enum ColumnType { Name = 0x0, Extension = 0x1, Size = 0x2, Mime = 0x3, DateTime = 0x4,
                          Permissions = 0x5, KrPermissions = 0x6, Owner = 0x7, Group = 0x8, MAX_COLUMNS = 0x03 };
	
	KrVfsModel( KrView * );
	virtual ~KrVfsModel();
	
	inline bool ready() const { return _vfs != 0; }
	void setVfs(vfs* v);
	
	int rowCount(const QModelIndex &parent = QModelIndex()) const;
	int columnCount(const QModelIndex &parent = QModelIndex()) const;
	QVariant data(const QModelIndex &index, int role) const;
	QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const;
	void setExtensionEnabled( bool exten ) { _extensionEnabled = exten; }
	inline const KrViewProperties * properties() const { return _view->properties(); }
	void sort() { sort( _lastSortOrder, _lastSortDir ); }
	void clear();
	virtual void sort ( int column, Qt::SortOrder order = Qt::AscendingOrder );
	
	
protected:
	vfs              * _vfs;
	QVector<vfile*>    _vfiles;
	bool               _extensionEnabled;
	KrView           * _view;
	int                _lastSortOrder;
	Qt::SortOrder      _lastSortDir;
};
#endif // __krvfsmodel__
