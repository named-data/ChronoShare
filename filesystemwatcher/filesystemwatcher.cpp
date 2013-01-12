#include "filesystemwatcher.h"
#include "ui_filesystemwatcher.h"

FileSystemWatcher::FileSystemWatcher(QString dirPath, QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::FileSystemWatcher),
    m_watcher(new QFileSystemWatcher()),
    m_listViewModel(new QStringListModel()),
    m_listView(new QListView()),
    m_timer(new QTimer(this)),
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

    // register signals (callback functions)
    connect(m_watcher, SIGNAL(directoryChanged(QString)), this, SLOT(watcherCallbackSlot(QString)));
    connect(m_timer, SIGNAL(timeout()), this, SLOT(timerCallbackSlot()));

    // bootstrap
    timerCallbackSlot();

    // start timer
    m_timer->start(60000);
}

FileSystemWatcher::~FileSystemWatcher()
{
    // clean up
    delete m_ui;
    delete m_watcher;
    delete m_listViewModel;
    delete m_listView;
    delete m_timer;
}

void FileSystemWatcher::watcherCallbackSlot(QString dirPath)
{
    // watcher specific steps
    qDebug() << endl << "[WATCHER] Triggered Path: " << dirPath;

    handleCallback(dirPath);
}

void FileSystemWatcher::timerCallbackSlot()
{
    // timer specific steps
    qDebug() << endl << "[TIMER] Triggered Path: " << m_dirPath;

    handleCallback(m_dirPath);
}

void FileSystemWatcher::handleCallback(QString dirPath)
{
    // scan directory and populate file list
    QHash<QString, sFileInfo> currentState = scanDirectory(dirPath);

    // reconcile directory and report changes
    QVector<sEventInfo> dirChanges = reconcileDirectory(currentState, dirPath);

    // DEBUG: Print to Gui
    printToGui(dirChanges);
}

QHash<QString, sFileInfo> FileSystemWatcher::scanDirectory(QString dirPath)
{   
    // list of files in directory
    QHash<QString, sFileInfo> currentState;

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
            QString absFilePath = fileInfo.absoluteFilePath();

            // if this is a directory
            if(fileInfo.isDir())
            {
                QStringList dirList = m_watcher->directories();

                // if the directory is not already being watched
                if (absFilePath.startsWith(m_dirPath) && !dirList.contains(absFilePath))
                {
                    // add this directory to the watch list
                    m_watcher->addPath(absFilePath);
                }
            }
            else
            {
                // construct struct
                sFileInfo fileInfoStruct;
                fileInfoStruct.fileInfo = fileInfo;
                fileInfoStruct.hash = calcChecksum(absFilePath);

                // add this file to the file list
                currentState.insert(absFilePath, fileInfoStruct);
            }
        }
    }

    return currentState;
}

QVector<sEventInfo> FileSystemWatcher::reconcileDirectory(QHash<QString, sFileInfo> currentState, QString dirPath)
{
    // list of files changed
    QVector<sEventInfo> dirChanges;

    // compare result (database/stored snapshot) to fileList (current snapshot)
    QMutableHashIterator<QString, sFileInfo> i(m_storedState);

    while(i.hasNext())
    {
        i.next();

        QString absFilePath = i.key();

        if(!absFilePath.startsWith(dirPath))
        {
            continue;
        }

        sFileInfo storedFileInfoStruct = i.value();
        QFileInfo storedFileInfo = storedFileInfoStruct.fileInfo;
        QByteArray storedHash = storedFileInfoStruct.hash;

        // check file existence
        if(currentState.contains(absFilePath))
        {
            sFileInfo currentFileInfoStruct = currentState.value(absFilePath);
            QFileInfo currentFileInfo = currentFileInfoStruct.fileInfo;
            QByteArray currentHash = currentFileInfoStruct.hash;

            if((storedFileInfo != currentFileInfo) || (storedHash != currentHash))
            {
                // update stored state
                i.setValue(currentFileInfoStruct);

                // this file has been modified
                sEventInfo eventInfo;
                eventInfo.event = MODIFIED;
                eventInfo.absFilePath = absFilePath;
                dirChanges.push_back(eventInfo);
            }

            // delete this file from fileList we have processed it
            currentState.remove(absFilePath);
        }
        else
        {
            // delete from stored state
            i.remove();

            // this file has been deleted
            sEventInfo eventInfo;
            eventInfo.event = DELETED;
            eventInfo.absFilePath = absFilePath;
            dirChanges.push_back(eventInfo);
        }
    }

    // any files left in fileList have been added
    for(QHash<QString, sFileInfo>::iterator i = currentState.begin(); i != currentState.end(); ++i)
    {
        QString absFilePath = i.key();
        sFileInfo currentFileInfoStruct = i.value();

        m_storedState.insert(absFilePath, currentFileInfoStruct);

        // this file has been added
        sEventInfo eventInfo;
        eventInfo.event = ADDED;
        eventInfo.absFilePath = absFilePath;
        dirChanges.push_back(eventInfo);
    }

    return dirChanges;
}

QByteArray FileSystemWatcher::calcChecksum(QString absFilePath)
{
    // initialize checksum
    QCryptographicHash crypto(QCryptographicHash::Md5);

    // open file
    QFile file(absFilePath);
    file.open(QFile::ReadOnly);

    // calculate checksum
    while(!file.atEnd())
    {
        crypto.addData(file.read(8192));
    }

    return crypto.result();
}

void FileSystemWatcher::printToGui(QVector<sEventInfo> dirChanges)
{
    if(!dirChanges.isEmpty())
    {
        QStringList dirChangesList;

        for(int i = 0; i < dirChanges.size(); i++)
        {
            QString tempString;

            eEvent event = dirChanges[i].event;
            QString absFilePath = dirChanges[i].absFilePath;

            switch(event)
            {
            case ADDED:
                tempString.append("ADDED: ");
                break;
            case MODIFIED:
                tempString.append("MODIFIED: ");
                break;
            case DELETED:
                tempString.append("DELETED: ");
                break;
            }

            tempString.append(absFilePath);

            dirChangesList.append(tempString);

            qDebug() << "\t" << tempString;
        }

        m_listViewModel->setStringList(dirChangesList);
    }
}

#if WAF
#include "filesystemwatcher.moc"
#include "filesystemwatcher.cpp.moc"
#endif
