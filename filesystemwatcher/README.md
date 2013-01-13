Overview:

FileSystemWatcher reports changes that are made to a monitored directory by signaling a registered callback function and passing that function a list of changes that have occurred.  Each element of the list represents a file and the action that was performed on that file. 

Example:

    ADDED: /Users/jared/Desktop/test.txt

The list is held in a vector of type sEventInfo, where sEventInfo is a struct defined as follows:

    enum eEvent {
        ADDED = 0,
        MODIFIED,
        DELETED
    };

    struct sEventInfo {
        eEvent event;
        std::string absFilePath;
    };

The eEvent enumerator specifies the action taken on the file and the string absFilePath is the absolute file path of the file.

Usage:

SimpleEventCatcher is a dummy class that serves as an example of how to register for signals from FileSystemWatcher.  These are the basic steps:

    // invoke file system watcher on specified path
    FileSystemWatcher watcher("/Users/jared/Desktop");

    // pass the instance of FileSystemWatcher to the class
    // that will register for event notifications
    SimpleEventCatcher dirEventCatcher(&watcher);

    // register for directory event signal (callback function)
    QObject::connect(watcher, SIGNAL(dirEventSignal(std::vector<sEventInfo>)), this,
                     SLOT(handleDirEvent(std::vector<sEventInfo>)));

    // implement handleDirEvent
    void SimpleEventCatcher::handleDirEvent(std::vector<sEventInfo>)
    {
        /* implementation here */
    }

Debug:

The debug flag can be set in filesystemwatcher.h.  It is set to 1 by default and outputs the following information to the console:

[BOOTSTRAP] 

[TIMER] Triggered Path:  "/Users/jared/Desktop" 
	 "ADDED: /Users/jared/Desktop/test2.txt" 
	 "ADDED: /Users/jared/Desktop/test.txt" 

[SIGNAL] From SimpleEventCatcher Slot: 
	 "ADDED: /Users/jared/Desktop/test2.txt" 
	 "ADDED: /Users/jared/Desktop/test.txt" 

[\BOOTSTRAP] 

[WATCHER] Triggered Path:  "/Users/jared/Desktop" 
	 "DELETED: /Users/jared/Desktop/test2.txt" 

[SIGNAL] From SimpleEventCatcher Slot: 
	 "DELETED: /Users/jared/Desktop/test2.txt" 
