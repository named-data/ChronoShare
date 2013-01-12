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
    connect(m_watcher, SIGNAL(directoryChanged(QString)), this, SLOT(handleCallback()));
    connect(m_timer, SIGNAL(timeout()), this, SLOT(handleCallback()));

    // bootstrap
    handleCallback();

    // start timer
    m_timer->start(10000);
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

void FileSystemWatcher::handleCallback()
{
    // scan directory and populate file list
    QHash<QString, sFileInfo> currentState = scanDirectory(m_dirPath);

    QStringList dirChanges = reconcileDirectory(currentState);

    // update gui with list of changes in this directory
    qDebug() << endl << m_watcher->directories() << endl;

    if(!dirChanges.isEmpty())
        m_listViewModel->setStringList(dirChanges);
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

                // initialize checksum
                QCryptographicHash crypto(QCryptographicHash::Md5);

                // open file
                QFile file(fileInfo.absoluteFilePath());
                file.open(QFile::ReadOnly);

                // calculate checksum
                while(!file.atEnd())
                {
                    crypto.addData(file.read(8192));
                }

                fileInfoStruct.hash = crypto.result();

                // add this file to the file list
                currentState.insert(absFilePath, fileInfoStruct);
            }
        }
    }

    return currentState;
}

QStringList FileSystemWatcher::reconcileDirectory(QHash<QString, sFileInfo> currentState)
{
    // list of files changed
    QStringList dirChanges;

    // compare result (database/stored snapshot) to fileList (current snapshot)
    QMutableHashIterator<QString, sFileInfo> i(m_storedState);
    while(i.hasNext())
    {
        i.next();

        QString absFilePath = i.key();

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
                dirChanges.append(absFilePath);
            }

            // delete this file from fileList we have processed it
            currentState.remove(absFilePath);
        }
        else
        {
            // delete from stored state
            i.remove();

            // this file has been deleted
            dirChanges.append(absFilePath);
        }
    }

    // any files left in fileList have been added
    for(QHash<QString, sFileInfo>::iterator i = currentState.begin(); i != currentState.end(); ++i)
    {
        QString absFilePath = i.key();
        sFileInfo currentFileInfoStruct = i.value();

        m_storedState.insert(absFilePath, currentFileInfoStruct);

        // this file has been added
        dirChanges.append(absFilePath);
    }

    return dirChanges;
}

#if WAF
#include "filesystemwatcher.moc"
#include "filesystemwatcher.cpp.moc"
#endif
