# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

VERSION='0.1'
APPNAME='chronoshare'
CCNXLIB='ccnx'

from waflib import Build, Logs

def options(opt):
    opt.add_option('--debug',action='store_true',default=False,dest='debug',help='''debugging mode''')
    opt.add_option('--test', action='store_true',default=False,dest='_test',help='''build unit tests''')
    opt.add_option('--yes',action='store_true',default=False) # for autoconf/automake/make compatibility

    opt.load('compiler_cxx boost ccnx protoc')
    opt.load('ice_cxx', tooldir=["."])

def configure(conf):
    conf.load("compiler_cxx")

    conf.define ("CHRONOSHARE_VERSION", VERSION)

    conf.check_cfg(package='sqlite3', args=['--cflags', '--libs'], uselib_store='SQLITE3', mandatory=True)

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
    conf.load('boost')

    conf.check_boost(lib='system iostreams regex thread')

    boost_version = conf.env.BOOST_VERSION.split('_')
    if int(boost_version[0]) < 1 or int(boost_version[1]) < 46:
        Logs.error ("Minumum required boost version is 1.46")
        return

    conf.check_ccnx (path=conf.options.ccnx_dir)

    if conf.options.debug:
        conf.define ('_DEBUG', 1)
        conf.env.append_value('CXXFLAGS', ['-O0', '-g3'])
    else:
        conf.env.append_value('CXXFLAGS', ['-O3', '-g'])

    if conf.options._test:
      conf.define('_TEST', 1)

    conf.load('protoc')
    conf.load('ice_cxx')
    
    conf.write_config_header('src/config.h')

def build (bld):
    libccnx = bld (
        target=CCNXLIB,
        features=['cxx', 'cxxshlib'],
        source =  [
            'src/ccnx-wrapper.cpp',
            'src/ccnx-pco.cpp',
            'src/ccnx-closure.cpp',
            'src/ccnx-tunnel.cpp',
            ],
        use = 'BOOST BOOST_THREAD SSL CCNX',
        includes = ['include', ],
        )
        
    # Unit tests
    if bld.get_define("_TEST"):
      unittests = bld.program (
          target="unit-tests",
          source = bld.path.ant_glob(['test/**/*.cc']),
          features=['cxx', 'cxxprogram'],
          use = 'BOOST_TEST',
          includes = ['include', ],
          )

    common = bld.objects (
        target = "common",
        features = ["cxx"],
        source = ['src/hash-helper.cc',
                  'src/chronoshare-client.ice',
                  ],
        use = 'BOOST',
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
                  'src/db-helper.cc',
                  'src/sync-log.cc',
                  'src/action-log.cc',
                  ],
        use = "BOOST CCNX SSL SQLITE3 ICE common",
        includes = ['include', 'src'],
        )
