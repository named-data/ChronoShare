# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

VERSION='0.1'
APPNAME='chronoshare'
CCNXLIB='ccnxx'

from waflib import Build, Logs

def options(opt):
    opt.add_option('--debug',action='store_true',default=False,dest='debug',help='''debugging mode''')
    opt.add_option('--test', action='store_true',default=False,dest='_test',help='''build unit tests''')
    opt.add_option('--yes',action='store_true',default=False) # for autoconf/automake/make compatibility

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
        conf.env.append_value('CXXFLAGS', ['-O0', '-Wall', '-Wno-unused-variable', '-fcolor-diagnostics', '-g3'])
    else:
        conf.env.append_value('CXXFLAGS', ['-O3', '-g'])

    if conf.options._test:
        conf.env.TEST = 1

    conf.write_config_header('src/config.h')

def build (bld):
    common = bld.objects (
        target = "common",
        features = ["cxx"],
        source = ['src/hash-helper.cc',
                  'src/chronoshare-client.ice',
                  ],
        use = 'BOOST',
        includes = ['include', 'src'],
        )

    libccnx = bld (
        target=CCNXLIB,
        features=['cxx', 'cxxshlib'],
        source =  [
            'src/ccnx-wrapper.cpp',
            'src/ccnx-pco.cpp',
            'src/ccnx-closure.cpp',
            'src/ccnx-tunnel.cpp',
            'src/object-db.cc',
            'src/object-manager.cc',
            'src/ccnx-name.cpp',
            'src/ccnx-selectors.cpp',
            'src/event-scheduler.cpp',
            ],
        use = 'BOOST BOOST_THREAD BOOST_FILESYSTEM SSL SQLITE3 CCNX common LIBEVENT LIBEVENT_PTHREADS',
        includes = ['include', 'src'],
        )

    database = bld.objects (
        target = "database",
        features = ["cxx"],
        source = [
                  'src/db-helper.cc',
                  'src/sync-log.cc',
                  'src/action-log.cc',
                  'src/action-item.proto',
                  'src/sync-state.proto',
            ],
        use = "BOOST SQLITE3 SSL common",
        includes = ['include', 'src'],
        )

    # Unit tests
    if bld.env['TEST']:
      unittests = bld.program (
          target="unit-tests",
          source = bld.path.ant_glob(['test/**/*.cc']),
          features=['cxx', 'cxxprogram'],
          use = 'BOOST_TEST ccnxx database',
          includes = ['include', 'src'],
          )

    client = bld (
        target="cs-client",
        features=['cxx', 'cxxprogram'],
        source = ['client/client.cc',
                  ],
        use = "BOOST CCNX SSL ICE common",
        includes = ['include', 'src'],
        )

    daemon = bld (
        target="cs-daemon",
        features=['cxx', 'cxxprogram'],
        # source = bld.path.ant_glob(['src/**/*.cc']),
        source = ['daemon/daemon.cc',
                  'daemon/notify-i.cc',
                  ],
        use = "BOOST CCNX SSL SQLITE3 ICE common database ccnxx",
        includes = ['include', 'src'],
        )
    


    qt = bld (
        target = "gui",
        features = "qt4 cxx cxxprogram",
        defines = "WAF",
        source = "filesystemwatcher/filesystemwatcher.cpp filesystemwatcher/filesystemwatcher.ui filesystemwatcher/main.cpp",
        includes = "filesystemwatcher src include .",
        use = "QTCORE QTGUI"
        )
