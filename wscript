# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

VERSION='0.1'
APPNAME='chronoshare'

from waflib import Build, Logs, Utils

def options(opt):
    opt.add_option('--debug',action='store_true',default=False,dest='debug',help='''debugging mode''')
    opt.add_option('--test', action='store_true',default=False,dest='_test',help='''build unit tests''')
    opt.add_option('--yes',action='store_true',default=False) # for autoconf/automake/make compatibility
    opt.add_option('--log4cxx', action='store_true',default=False,dest='log4cxx',help='''Compile with log4cxx logging support''')

    opt.load('compiler_c compiler_cxx boost ccnx protoc qt4')

def configure(conf):
    conf.load("compiler_c compiler_cxx")

    conf.define ("CHRONOSHARE_VERSION", VERSION)

    conf.check_cfg(package='sqlite3', args=['--cflags', '--libs'], uselib_store='SQLITE3', mandatory=True)
    conf.check_cfg(package='libevent', args=['--cflags', '--libs'], uselib_store='LIBEVENT', mandatory=True)
    conf.check_cfg(package='libevent_pthreads', args=['--cflags', '--libs'], uselib_store='LIBEVENT_PTHREADS', mandatory=True)

    if Utils.unversioned_sys_platform () == "darwin":
        conf.check_cxx(framework_name='Foundation', uselib_store='OSX_FOUNDATION', cxxflags="-ObjC++", mandatory=False)
        conf.check_cxx(framework_name='CoreWLAN',   uselib_store='OSX_COREWLAN',   cxxflags="-ObjC++", define_name='HAVE_COREWLAN', mandatory=False)

    if not conf.check_cfg(package='openssl', args=['--cflags', '--libs'], uselib_store='SSL', mandatory=False):
        libcrypto = conf.check_cc(lib='crypto',
                                  header_name='openssl/crypto.h',
                                  define_name='HAVE_SSL',
                                  uselib_store='SSL')
    else:
        conf.define ("HAVE_SSL", 1)
    if not conf.get_define ("HAVE_SSL"):
        conf.fatal ("Cannot find SSL libraries")

    if conf.options.log4cxx:
        conf.check_cfg(package='liblog4cxx', args=['--cflags', '--libs'], uselib_store='LOG4CXX', mandatory=True)
        conf.define ("HAVE_LOG4CXX", 1)

    conf.load ('ccnx')

    conf.load('protoc')

    conf.load('qt4')

    conf.load('boost')

    conf.check_boost(lib='system test iostreams filesystem regex thread')

    boost_version = conf.env.BOOST_VERSION.split('_')
    if int(boost_version[0]) < 1 or int(boost_version[1]) < 46:
        Logs.error ("Minumum required boost version is 1.46")
        return

    conf.check_ccnx (path=conf.options.ccnx_dir)
    conf.define ('CCNX_PATH', conf.env.CCNX_ROOT)

    if conf.options.debug:
        conf.define ('_DEBUG', 1)
        conf.env.append_value('CXXFLAGS', ['-O0', '-Wall', '-Wno-unused-variable', '-g3'])
    else:
        conf.env.append_value('CXXFLAGS', ['-O3', '-g'])

    if conf.env["CXX"] == ["clang++"]:
        conf.env.append_value('CXXFLAGS', ['-fcolor-diagnostics', '-Qunused-arguments'])

    if conf.options._test:
        conf.define ('_TESTS', 1)
        conf.env.TEST = 1

    conf.write_config_header('src/config.h')

def build (bld):
    executor = bld.objects (
        target = "executor",
        features = ["cxx"],
        source = bld.path.ant_glob(['executor/**/*.cc']),
        use = 'BOOST BOOST_THREAD LIBEVENT LIBEVENT_PTHREADS LOG4CXX',
        includes = "executor src",
        )

    scheduler = bld.objects (
        target = "scheduler",
        features = ["cxx"],
        source = bld.path.ant_glob(['scheduler/**/*.cc']),
        use = 'BOOST BOOST_THREAD LIBEVENT LIBEVENT_PTHREADS LOG4CXX executor',
        includes = "scheduler executor src",
        )

    libccnx = bld (
        target="ccnx",
        features=['cxx'],
        source = bld.path.ant_glob(['ccnx/**/*.cc', 'ccnx/**/*.cpp']),
        use = 'BOOST BOOST_THREAD SSL CCNX LOG4CXX scheduler executor',
        includes = "ccnx src scheduler executor",
        )

    adhoc = bld (
        target = "adhoc",
        features=['cxx'],
        includes = "ccnx src",
    )
    if Utils.unversioned_sys_platform () == "darwin":
        adhoc.mac_app = True
        adhoc.source = 'adhoc/adhoc-osx.mm'
        adhoc.use = "LOG4CXX OSX_FOUNDATION OSX_COREWLAN"

    chornoshare = bld (
        target="chronoshare",
        features=['cxx'],
        source = bld.path.ant_glob(['src/**/*.cc', 'src/**/*.cpp', 'src/**/*.proto']),
        use = "BOOST BOOST_FILESYSTEM SQLITE3 LOG4CXX scheduler ccnx",
        includes = "ccnx scheduler src executor",
        )

    fs_watcher = bld (
        target = "fs_watcher",
        features = "qt4 cxx",
        defines = "WAF",
        source = bld.path.ant_glob(['fs-watcher/*.cc']),
        use = "SQLITE3 LOG4CXX scheduler executor QTCORE",
        includes = "fs-watcher scheduler executor src",
        )

    # Unit tests
    if bld.env['TEST']:
      unittests = bld.program (
          target="unit-tests",
          features = "qt4 cxx cxxprogram",
          defines = "WAF",
          source = bld.path.ant_glob(['test/*.cc']),
          use = 'BOOST_TEST BOOST_FILESYSTEM LOG4CXX SQLITE3 QTCORE QTGUI ccnx database fs_watcher chronoshare',
          includes = "ccnx scheduler src executor gui fs-watcher",
          install_prefix = None,
          )

    http_server = bld (
          target = "http_server",
          features = "qt4 cxx",
          source = bld.path.ant_glob(['server/*.cpp']),
          includes = "server src .",
          use = "BOOST QTCORE"
          )

    qt = bld (
        target = "ChronoShare",
        features = "qt4 cxx cxxprogram",
        defines = "WAF",
        # do not include html.qrc as we don't want it to be compiled into binary
        # qt seems to adopt a pattern of compiling every resource file into the
        # executable; if things don't work, we can use that as last resort
        source = bld.path.ant_glob(['gui/*.cpp', 'gui/*.cc', 'gui/images.qrc']),
        includes = "ccnx scheduler executor fs-watcher gui src adhoc server . ",
        use = "BOOST BOOST_FILESYSTEM SQLITE3 QTCORE QTGUI LOG4CXX fs_watcher ccnx database chronoshare http_server"
        )

    if Utils.unversioned_sys_platform () == "darwin":
        app_plist = '''<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist SYSTEM "file://localhost/System/Library/DTDs/PropertyList.dtd">
<plist version="0.9">
<dict>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleGetInfoString</key>
    <string>Created by Waf</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>NOTE</key>
    <string>THIS IS A GENERATED FILE, DO NOT MODIFY</string>
    <key>CFBundleExecutable</key>
    <string>%s</string>
    <key>LSUIElement</key>
    <string>1</string>
</dict>
</plist>'''
        qt.mac_app = "ChronoShare.app"
        qt.mac_plist = app_plist % "ChronoShare"
        qt.use += " OSX_FOUNDATION OSX_COREWLAN adhoc"
        # qt.use += " OSX_FOUNDATION OSX_COREWLAN adhoc"


    cmdline = bld (
        target = "csd",
	features = "qt4 cxx cxxprogram",
	defines = "WAF",
	source = "cmd/csd.cc",
	includes = "ccnx scheduler executor gui fs-watcher src . ",
	use = "BOOST BOOST_FILESYSTEM SQLITE3 QTCORE QTGUI LOG4CXX fs_watcher ccnx database chronoshare"
	)

    dump_db = bld (
        target = "dump-db",
        features = "cxx cxxprogram",
	source = "cmd/dump-db.cc",
	includes = "ccnx scheduler executor gui fs-watcher src . ",
	use = "BOOST BOOST_FILESYSTEM SQLITE3 QTCORE LOG4CXX fs_watcher ccnx database chronoshare"
        )

from waflib import TaskGen
@TaskGen.extension('.mm')
def m_hook(self, node):
    """Alias .mm files to be compiled the same as .cc files, gcc/clang will do the right thing."""
    return self.create_compiled_task('cxx', node)
