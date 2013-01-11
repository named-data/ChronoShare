#include "filesystemwatcher.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    // invoke file system watcher on specified path
    FileSystemWatcher watcher("/Users/jared/Desktop");
    watcher.show();

    return app.exec();
}
