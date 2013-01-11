#include "filesystemwatcher.h"
#include "ui_filesystemwatcher.h"

FileSystemWatcher::FileSystemWatcher(QString dirPath, QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::FileSystemWatcher),
    m_watcher(new QFileSystemWatcher()),
    m_listViewModel(new QStringListModel()),
    m_listView(new QListView()),
    m_dirPath(dirPath)
{
    // setup user interface
    m_ui->setupUi(this);

    // add main directory to monitor
    m_watcher->addPath(m_dirPath);

    // create the view
    m_listView->setModel(m_listViewModel);
    setCentralWidget(m_listView);

    // set title
    setWindowTitle("ChronoShare");

    // open database
    m_db = QSqlDatabase::addDatabase("QSQLITE");
    m_db.setDatabaseName("filesystem.db");

    if(!m_db.open())
    {
        qDebug() << "Error: Could not open database.";
        return;
    }

    if(!createFileTable())
    {
        qDebug() << "Error: Could not create table.";
        return;
    }

    // register signals (callback functions)
    connect(m_watcher, SIGNAL(directoryChanged(QString)),this,SLOT(dirChangedSlot(QString)));

    // bootstrap file list
    dirChangedSlot(m_dirPath);
}

FileSystemWatcher::~FileSystemWatcher()
{
    // clean up
    delete m_ui;
    delete m_watcher;
    delete m_listViewModel;
    delete m_listView;

    // close database
    m_db.close();
}

void FileSystemWatcher::dirChangedSlot(QString dirPath)
{
    // scan directory and populate file list
    QHash<QString, QFileInfo> fileList = scanDirectory(dirPath);

    QStringList dirChanges = reconcileDirectory(fileList);

    // update gui with list of changes in this directory
    m_listViewModel->setStringList(dirChanges);
}

QHash<QString, QFileInfo> FileSystemWatcher::scanDirectory(QString dirPath)
{
    // list of files in directory
    QHash<QString, QFileInfo> fileList;

    // directory iterator (recursive)
    QDirIterator dirIterator(dirPath, QDirIterator::Subdirectories |
                                        QDirIterator::FollowSymlinks);

    // iterate through directory recursively
    while(dirIterator.hasNext())
    {
        // Get Next File/Dir
        dirIterator.next();

        // Get FileInfo
        QFileInfo fileInfo = dirIterator.fileInfo();

        // if not this directory or previous directory
        if(fileInfo.absoluteFilePath() != ".." && fileInfo.absoluteFilePath() != ".")
        {
            // if this is a directory
            if(fileInfo.isDir())
            {
                QStringList dirList = m_watcher->directories();

                // if the directory is not already being watched
                if (!dirList.contains(fileInfo.absoluteFilePath()))
                {
                    // add this directory to the watch list
                    m_watcher->addPath(fileInfo.absoluteFilePath());
                }
            }
            else
            {
                // add this file to the file list
                fileList.insert(fileInfo.absoluteFilePath(), fileInfo);
            }
        }
    }

    return fileList;
}

QStringList FileSystemWatcher::reconcileDirectory(QHash<QString, QFileInfo> fileList)
{
    QStringList dirChanges;

    // setup database query
    QSqlQuery storedRecord(m_db);

    // query database for list of files
    storedRecord.exec("SELECT absFilePath, lastModified FROM files");

    // Debug
    qDebug() << storedRecord.lastQuery();

    // compare result (database/stored snapshot) to fileList (current snapshot)
    while(storedRecord.next())
    {
        QString absFilePath = storedRecord.value(0).toString();
        qint64 lMStored = storedRecord.value(1).toLongLong();

        // debug
        qDebug() << absFilePath << ", " << lMStored;

        // check file existence
        /*if(fileList.contains(absFilePath))
        {
            QFileInfo fileInfo = fileList.value(absFilePath);

            // last Modified
            qint64 lMCurrent = fileInfo.lastModified().currentMSecsSinceEpoch();

            if(lMStored != lMCurrent)
            {
                storedRecord.prepare("UPDATE files SET lastModified = :lastModified "
                              "WHERE absFilePath= :absFilePath");
                storedRecord.bindValue(":lastModified", lMCurrent);
                storedRecord.bindValue(":absFilePath", absFilePath);
                storedRecord.exec();

                // Debug
                qDebug() << storedRecord.lastQuery();

                // this file has been modified
                dirChanges.append(absFilePath);
            }

            // delete this file from fileList, we have processed it
            fileList.remove(absFilePath);
        }
        else
        {
            storedRecord.prepare("DELETE FROM files WHERE absFilePath= :absFilePath");
            storedRecord.bindValue(":absFilePath", absFilePath);
            storedRecord.exec();

            // Debug
            qDebug() << storedRecord.lastQuery();

            // this file has been deleted
            dirChanges.append(absFilePath);
        }*/
    }

    /*storedRecord.prepare("INSERT INTO files (absFilePath, lastModified) "
                  "VALUES (:absFilePath, :lastModified)");

    // any files left in fileList have been added
    for(QHash<QString, QFileInfo>::iterator i = fileList.begin(); i != fileList.end(); ++i)
    {
        QString absFilePath = i.key();
        qint64 lastModified = i.value().lastModified().currentMSecsSinceEpoch();

        storedRecord.bindValue(":absFilePath", absFilePath);
        storedRecord.bindValue(":lastModified", lastModified);
        storedRecord.exec();

        // this file has been added
        dirChanges.append(absFilePath);
    }*/

    // close query
    storedRecord.finish();

    return dirChanges;
}

bool FileSystemWatcher::createFileTable()
{
    bool success = false;

    if(m_db.isOpen())
    {
        if(m_db.tables().contains("files"))
        {
            success = true;
        }
        else
        {
            QSqlQuery query(m_db);

            // create file table
            success = query.exec("CREATE TABLE files "
                                 "(absFilePath TEXT primary key, "
                                 "lastModified UNSIGNED BIG INT, "
                                 "fileSize UNSIGNED BIG INT)");

            // Debug
            qDebug() << query.lastQuery();

            query.finish();
        }
    }

    return success;
}

#if WAF
#include "filesystemwatcher.moc"
#include "filesystemwatcher.cpp.moc"
#endif
