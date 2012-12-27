# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

VERSION='0.1'
APPNAME='chronoshare'

from waflib import Build, Logs

def options(opt):
    opt.load('compiler_cxx boost ccnx')
    opt.add_option('--debug',action='store_true',default=False,dest='debug',help='''debugging mode''')
    opt.add_option('--yes',action='store_true',default=False) # for autoconf/automake/make compatibility

def configure(conf):
    conf.load("compiler_cxx boost ccnx")

    conf.check_cfg(package='sqlite3', args=['--cflags', '--libs'], uselib_store='SQLITE3', mandatory=True)

    if not conf.check_cfg(package='openssl', args=['--cflags', '--libs'], uselib_store='SSL', mandatory=False):
        Logs.error ("bla")
        conf.check_cc(lib='crypto',
                      header_name='openssl/crypto.h',
                      define_name='HAVE_SSL',
                      uselib_store='SSL')
    else:
        conf.define ("HAVE_SSL", 1)
    
    if not conf.get_define ("HAVE_SSL"):
        conf.fatal ("Cannot find SSL libraries")

    conf.check_boost(lib='system iostreams regex')

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

    conf.write_config_header('src/config.h')

def build (bld):
    chronoshare = bld (
        target=APPNAME,
        features=['cxx', 'cxxprogram'],
        # source = bld.path.ant_glob(['src/**/*.cc']),
        source = ['src/main.cc', 'src/sqlite-helper.cc'],
        use = 'BOOST BOOST_IOSTREAMS BOOST_REGEX CCNX SSL SQLITE3',
        includes = ['include', 'src'],
        )
