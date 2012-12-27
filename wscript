# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

VERSION='0.0.1'
APPNAME='chronoshare'

from waflib import Build, Logs

def options(opt):
    opt.add_option('--debug',action='store_true',default=False,dest='debug',help='''debugging mode''')
    opt.add_option('--test', action='store_true',default=False,dest='_test',help='''build unit tests''')
    opt.load('compiler_c')
    opt.load('compiler_cxx')
    opt.load('boost')
    opt.load('gnu_dirs')
    opt.load('ccnx protobuf', tooldir=["waf-tools"])

def configure(conf):
    conf.load("compiler_cxx")
    conf.load('gnu_dirs')

    if not conf.check_cfg(package='openssl', args=['--cflags', '--libs'], uselib_store='SSL', mandatory=False):
      libcrypto = conf.check_cc(lib='crypto',
                                header_name='openssl/crypto.h',
                                define_name='HAVE_SSL',
                                uselib_store='SSL')
    if not conf.get_define ("HAVE_SSL"):
        conf.fatal ("Cannot find SSL libraries")

    conf.load('boost')

    conf.check_boost(lib='system test thread')

    conf.load ('ccnx')
    conf.check_ccnx (path=conf.options.ccnx_dir)

    if conf.options.debug:
        conf.define ('_DEBUG', 1)
        conf.env.append_value('CXXFLAGS', ['-O0', '-g3'])
    else:
        conf.env.append_value('CXXFLAGS', ['-O3', '-g'])

    if conf.options._test:
      conf.define('_TEST', 1)

    conf.load('protobuf')

def build (bld):
    bld.post_mode = Build.POST_LAZY

    bld.add_group ("protobuf")

#    x = bld (
#        features = ["protobuf"],
#        source = ["model/sync-state.proto"],
#        target = ["model/sync-state.pb"],
#        )

    bld.add_group ("code")
        
    libccnx = bld (
        target=APPNAME,
        features=['cxx', 'cxxshlib'],
        source =  [
            'src/ccnx-wrapper.cpp',
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




