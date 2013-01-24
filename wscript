# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

VERSION='0.1'
APPNAME='chronoshare'

from waflib import Build, Logs

def options(opt):
    opt.add_option('--debug',action='store_true',default=False,dest='debug',help='''debugging mode''')
    opt.add_option('--test', action='store_true',default=False,dest='_test',help='''build unit tests''')
    opt.add_option('--yes',action='store_true',default=False) # for autoconf/automake/make compatibility
    opt.add_option('--log4cxx', action='store_true',default=False,dest='log4cxx',help='''Compile with log4cxx logging support''')

    opt.load('compiler_cxx boost ccnx protoc ice_cxx qt4')

def configure(conf):
    conf.load("compiler_cxx")

    conf.define ("CHRONOSHARE_VERSION", VERSION)

    conf.check_cfg(package='sqlite3', args=['--cflags', '--libs'], uselib_store='SQLITE3', mandatory=True)
    conf.check_cfg(package='libevent', args=['--cflags', '--libs'], uselib_store='LIBEVENT', mandatory=True)
    conf.check_cfg(package='libevent_pthreads', args=['--cflags', '--libs'], uselib_store='LIBEVENT_PTHREADS', mandatory=True)

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
    conf.load('ice_cxx')

    conf.load('qt4')

    conf.load('boost')

    conf.check_boost(lib='system test iostreams filesystem regex thread')

    boost_version = conf.env.BOOST_VERSION.split('_')
    if int(boost_version[0]) < 1 or int(boost_version[1]) < 46:
        Logs.error ("Minumum required boost version is 1.46")
        return

    conf.check_ccnx (path=conf.options.ccnx_dir)

    if conf.options.debug:
        conf.define ('_DEBUG', 1)
        conf.env.append_value('CXXFLAGS', ['-O0', '-Wall', '-Wno-unused-variable',
                                           '-fcolor-diagnostics', '-g3', '-Qunused-arguments'])
    else:
        conf.env.append_value('CXXFLAGS', ['-O3', '-g', '-Qunused-arguments'])

    if conf.options._test:
        conf.define ('_TESTS', 1)
        conf.env.TEST = 1

    conf.write_config_header('src/config.h')

def build (bld):
    scheduler = bld.objects (
        target = "scheduler",
        features = ["cxx"],
        source = bld.path.ant_glob(['scheduler/**/*.cc']),
        use = 'BOOST BOOST_THREAD LIBEVENT LIBEVENT_PTHREADS LOG4CXX',
        includes = "scheduler",
        )

    libccnx = bld (
        target="ccnx",
        features=['cxx'],
        source = bld.path.ant_glob(['ccnx/**/*.cc', 'ccnx/**/*.cpp']),
        use = 'BOOST BOOST_THREAD SSL CCNX LOG4CXX scheduler',
        includes = "ccnx scheduler",
        )

    chornoshare = bld (
        target="chronoshare",
        features=['cxx'],
        source = bld.path.ant_glob(['src/**/*.cc', 'src/**/*.cpp', 'src/**/*.proto']),
        use = "BOOST BOOST_FILESYSTEM SQLITE3 LOG4CXX scheduler ccnx",
        includes = "ccnx scheduler src",
        )

    # Unit tests
    if bld.env['TEST']:
      unittests = bld.program (
          target="unit-tests",
          source = bld.path.ant_glob(['test/*.cc']),
          features=['cxx', 'cxxprogram'],
          use = 'BOOST_TEST BOOST_FILESYSTEM LOG4CXX ccnx database chronoshare',
          includes = "ccnx scheduler src",
          )

    qt = bld (
        target = "fs-watcher",
        features = "qt4 cxx",
        defines = "WAF",
          source = bld.path.ant_glob(['fs-watcher/*.cc']),
        includes = "fs-watcher . src ",
        use = "QTCORE QTGUI LOG4CXX"
        )

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

    qt = bld (
	target = "ChronoShare",
        mac_app = "ChronoShare.app",
        mac_plist = app_plist % "ChronoShare",
	features = "qt4 cxx cxxprogram",
	defines = "WAF",
	source = bld.path.ant_glob(['gui/*.cpp', 'gui/*.qrc']),
	includes = "src gui fs-watcher src . ",
	use = "QTCORE QTGUI LOG4CXX fs-watcher ccnx database chronoshare"
	)
