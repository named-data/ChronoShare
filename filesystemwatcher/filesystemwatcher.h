#ifndef FILESYSTEMWATCHER_H
#define FILESYSTEMWATCHER_H

#include <QtGui>

namespace Ui {
class filesystemwatcher;
}

class filesystemwatcher : public QMainWindow
{
    Q_OBJECT
    
public:
    // constructor
    filesystemwatcher(QString dirPath, QWidget *parent = 0);

    // destructor
    ~filesystemwatcher();

private slots:
    // signal for changes to monitored files
    void fileChangedSlot(QString filePath);

    // signal for changes to monitored directories
    void dirChangedSlot(QString dirPath);
    
private:
    Ui::filesystemwatcher* m_ui; // user interface
    QFileSystemWatcher* m_watcher; // filesystem watcher
    QStringListModel* m_listViewModel; // list view model
    QListView* m_listView; // list
    QString m_dirPath; // monitored path
};

#endif // FILESYSTEMWATCHER_H
