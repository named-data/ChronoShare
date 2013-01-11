#ifndef FILESYSTEMWATCHER_H
#define FILESYSTEMWATCHER_H

#include <QtGui>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlResult>
#include <QDebug>
#include <QHash>

namespace Ui {
class FileSystemWatcher;
}

class FileSystemWatcher : public QMainWindow
{
    Q_OBJECT

public:
    // constructor
    FileSystemWatcher(QString dirPath, QWidget *parent = 0);

    // destructor
    ~FileSystemWatcher();

private slots:
    // signal for changes to monitored directories
    void dirChangedSlot(QString dirPath);

private:
    // scan directory and populate file list
    QHash<QString, QFileInfo> scanDirectory(QString filePath);

    // reconcile directory, find changes
    QStringList reconcileDirectory(QHash<QString, QFileInfo> fileList);

    // create file table in database
    bool createFileTable();

private:
    Ui::FileSystemWatcher* m_ui; // user interface
    QFileSystemWatcher* m_watcher; // filesystem watcher
    QStringListModel* m_listViewModel; // list view model
    QListView* m_listView; // list
    QString m_dirPath; // monitored path

    QSqlDatabase m_db; // filesystem database
};

#endif // FILESYSTEMWATCHER_H
