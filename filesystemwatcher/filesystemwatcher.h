#ifndef FILESYSTEMWATCHER_H
#define FILESYSTEMWATCHER_H

#include <QtGui>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlResult>
#include <QSqlError>
#include <QDebug>
#include <QHash>
#include <QCryptographicHash>
#include <QVector>

enum eEvent {
    ADDED = 0,
    MODIFIED,
    DELETED
};

struct sEventInfo {
    eEvent event;
    QString absFilePath;
};

struct sFileInfo {
    QByteArray hash;
    QFileInfo fileInfo;
};

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
    // handle callback from watcher
    void watcherCallbackSlot(QString dirPath);

    // handle callback from timer
    void timerCallbackSlot();

private:
    // handle callback from either the watcher or timer
    void handleCallback(QString dirPath);

    // scan directory and populate file list
    QHash<QString, sFileInfo> scanDirectory(QString dirPath);

    // reconcile directory, find changes
    QVector<sEventInfo> reconcileDirectory(QHash<QString, sFileInfo> fileList, QString dirPath);

    // calculate checksum
    QByteArray calcChecksum(QString absFilePath);

    // print to GUI (DEBUG)
    void printToGui(QVector<sEventInfo> dirChanges);

private:
    Ui::FileSystemWatcher* m_ui; // user interface
    QFileSystemWatcher* m_watcher; // filesystem watcher
    QStringListModel* m_listViewModel; // list view model
    QListView* m_listView; // list
    QTimer* m_timer; // timer

    QString m_dirPath; // monitored path
    QHash<QString, sFileInfo> m_storedState; // stored state of directory
};

#endif // FILESYSTEMWATCHER_H
