#include "filesystemwatcher.h"
#include "ui_filesystemwatcher.h"

filesystemwatcher::filesystemwatcher(QString dirPath, QWidget *parent) :
    QMainWindow(parent),
    m_ui(new Ui::filesystemwatcher),
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

    // register signals (callback functions)
    connect(m_watcher,SIGNAL(fileChanged(QString)),this,SLOT(fileChangedSlot(QString)));
    connect(m_watcher,SIGNAL(directoryChanged(QString)),this,SLOT(dirChangedSlot(QString)));

    // bootstrap file list
    dirChangedSlot(m_dirPath);
}

filesystemwatcher::~filesystemwatcher()
{
    // clean up
    delete m_ui;
    delete m_watcher;
    delete m_listViewModel;
    delete m_listView;
}

void filesystemwatcher::fileChangedSlot(QString filePath)
{
    QStringList fileList;
    fileList.append(filePath);
}

void filesystemwatcher::dirChangedSlot(QString dirPath)
{
    // list of files in directory
    QStringList fileList;

    // directory iterator (recursive)
    QDirIterator dirIterator(m_dirPath, QDirIterator::Subdirectories |
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
                fileList.append(fileInfo.absoluteFilePath());
            }
        }
    }

    // update gui with list of files in this directory
    m_listViewModel->setStringList(fileList);
}
