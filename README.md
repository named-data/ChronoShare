ChronoShare: Decentralized File Sharing Over NDN
================================================

ChronoShare provides services similar to Dropbox, but in a decentralized way.

It uses [ChronoSync](http://www.named-data.net/techreport/TR008-chronos.pdf) library to synchronize the operations to the shared-folder and levels NDN's advantage of natural multicast support. The sharing process is completely decentralized, but it is also very easy to add a permanent storage server.

To see more details about ChronoShare design, click [here](http://irl.cs.ucla.edu/~zhenkai/chronoshare.pdf).

Highlights
----------
- Decentralised
- Version controlled
- Wifi Adhoc communications (Only supported in Mac OS 10.7 and above)
- NDN-JS interface for versioning history browsing and checking out old version
- Dropbox like user experience (ok, their UI is fancier)

Compile
-------
To see more configure options, use `./waf configure`.
To compile with default configuration, use
```bash
./waf configure
./waf
```
