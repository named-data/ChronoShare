# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-
VERSION='0.4'
APPNAME='chronoshare'

from waflib import Build, Logs, Utils, Task, TaskGen, Configure

def options(opt):
    opt.add_option('--test', action='store_true',default=False,dest='_test',help='''build unit tests''')
    opt.add_option('--yes',action='store_true',default=False) # for autoconf/automake/make compatibility
    opt.add_option('--log4cxx', action='store_true',default=False,dest='log4cxx',help='''Compile with log4cxx logging support''')

    if Utils.unversioned_sys_platform () == "darwin":
        opt.add_option('--auto-update', action='store_true',default=False,dest='autoupdate',help='''(OSX) Download sparkle framework and enable autoupdate feature''')

    opt.load('compiler_c compiler_cxx boost protoc qt4 gnu_dirs')
    opt.load('ndnx flags tinyxml', tooldir=['waf-tools'])

def configure(conf):
    conf.load("compiler_c compiler_cxx gnu_dirs flags")

    conf.define ("CHRONOSHARE_VERSION", VERSION)

    conf.check_cfg(package='sqlite3', args=['--cflags', '--libs'], uselib_store='SQLITE3', mandatory=True)
    conf.check_cfg(package='libevent', args=['--cflags', '--libs'], uselib_store='LIBEVENT', mandatory=True)
    conf.check_cfg(package='libevent_pthreads', args=['--cflags', '--libs'], uselib_store='LIBEVENT_PTHREADS', mandatory=True)
    conf.load('tinyxml')
    conf.check_tinyxml(path=conf.options.tinyxml_dir)

    conf.define ("TRAY_ICON", "chronoshare-big.png")
    if Utils.unversioned_sys_platform () == "linux":
        conf.define ("TRAY_ICON", "chronoshare-ubuntu.png")

    if Utils.unversioned_sys_platform () == "darwin":
        conf.check_cxx(framework_name='Foundation', uselib_store='OSX_FOUNDATION', mandatory=False, compile_filename='test.mm')
        conf.check_cxx(framework_name='AppKit',     uselib_store='OSX_APPKIT',     mandatory=False, compile_filename='test.mm')
        conf.check_cxx(framework_name='CoreWLAN',   uselib_store='OSX_COREWLAN',   define_name='HAVE_COREWLAN',
                       use="OSX_FOUNDATION", mandatory=False, compile_filename='test.mm')

        if conf.options.autoupdate:
            def check_sparkle(**kwargs):
              conf.check_cxx (framework_name="Sparkle", header_name=["Foundation/Foundation.h", "AppKit/AppKit.h"],
                              uselib_store='OSX_SPARKLE', define_name='HAVE_SPARKLE', mandatory=True,
                              compile_filename='test.mm', use="OSX_FOUNDATION OSX_APPKIT",
                              **kwargs
                              )
            try:
                # Try standard paths first
                check_sparkle()
            except:
                try:
                    # Try local path
                    Logs.info ("Check local version of Sparkle framework")
                    check_sparkle(cxxflags="-F%s/osx/Frameworks/" % conf.path.abspath(),
                                  linkflags="-F%s/osx/Frameworks/" % conf.path.abspath())
                    conf.env.HAVE_LOCAL_SPARKLE = 1
                except:
                    import urllib, subprocess, os, shutil
                    if not os.path.exists('osx/Frameworks/Sparkle.framework'):
                        # Download to local path and retry
                        Logs.info ("Sparkle framework not found, trying to download it to 'build/'")

                        urllib.urlretrieve ("http://sparkle.andymatuschak.org/files/Sparkle%201.5b6.zip", "build/Sparkle.zip")
                        if os.path.exists('build/Sparkle.zip'):
                            try:
                                subprocess.check_call (['unzip', '-qq', 'build/Sparkle.zip', '-d', 'build/Sparkle'])
                                os.remove ("build/Sparkle.zip")
                                if not os.path.exists("osx/Frameworks"):
                                    os.mkdir ("osx/Frameworks")
                                os.rename ("build/Sparkle/Sparkle.framework", "osx/Frameworks/Sparkle.framework")
                                shutil.rmtree("build/Sparkle", ignore_errors=True)

                                check_sparkle(cxxflags="-F%s/osx/Frameworks/" % conf.path.abspath(),
                                              linkflags="-F%s/osx/Frameworks/" % conf.path.abspath())
                                conf.env.HAVE_LOCAL_SPARKLE = 1
                            except subprocess.CalledProcessError as e:
                                conf.fatal("Cannot find Sparkle framework. Auto download failed: '%s' returned %s" % (' '.join(e.cmd), e.returncode))
                            except:
                                conf.fatal("Unknown Error happened when auto downloading Sparkle framework")

            if conf.is_defined('HAVE_SPARKLE'):
                conf.env.HAVE_SPARKLE = 1 # small cheat for wscript

    if conf.options.log4cxx:
        conf.check_cfg(package='liblog4cxx', args=['--cflags', '--libs'], uselib_store='LOG4CXX', mandatory=True)
        conf.define ("HAVE_LOG4CXX", 1)

    conf.load ('ndnx')

    conf.load('protoc')

    conf.load('qt4')

    conf.load('boost')

    conf.check_boost(lib='system test iostreams filesystem regex thread date_time')

    boost_version = conf.env.BOOST_VERSION.split('_')
    if int(boost_version[0]) < 1 or int(boost_version[1]) < 46:
        Logs.error ("Minumum required boost version is 1.46")
        return

    conf.check_ndnx ()
    conf.check_openssl ()
    conf.define ('NDNX_PATH', conf.env.NDNX_ROOT)

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

    libndnx = bld (
        target="ndnx",
        features=['cxx'],
        source = bld.path.ant_glob(['ndnx/**/*.cc', 'ndnx/**/*.cpp']),
        use = 'TINYXML BOOST BOOST_THREAD SSL NDNX LOG4CXX scheduler executor',
        includes = "ndnx src scheduler executor",
        )

    adhoc = bld (
        target = "adhoc",
        features=['cxx'],
        includes = "ndnx src",
    )
    if Utils.unversioned_sys_platform () == "darwin":
        adhoc.mac_app = True
        adhoc.source = 'adhoc/adhoc-osx.mm'
        adhoc.use = "BOOST BOOST_THREAD BOOST_DATE_TIME LOG4CXX OSX_FOUNDATION OSX_COREWLAN"

    chornoshare = bld (
        target="chronoshare",
        features=['cxx'],
        source = bld.path.ant_glob(['src/**/*.cc', 'src/**/*.cpp', 'src/**/*.proto']),
        use = "BOOST BOOST_FILESYSTEM BOOST_DATE_TIME SQLITE3 LOG4CXX scheduler ndnx",
        includes = "ndnx scheduler src executor",
        )

    fs_watcher = bld (
        target = "fs_watcher",
        features = "qt4 cxx",
        defines = "WAF",
        source = bld.path.ant_glob(['fs-watcher/*.cc']),
        use = "SQLITE3 LOG4CXX scheduler executor QTCORE",
        includes = "ndnx fs-watcher scheduler executor src",
        )

    # Unit tests
    if bld.env['TEST']:
      unittests = bld.program (
          target="unit-tests",
          features = "qt4 cxx cxxprogram",
          defines = "WAF",
          source = bld.path.ant_glob(['test/*.cc']),
          use = 'BOOST_TEST BOOST_FILESYSTEM BOOST_DATE_TIME LOG4CXX SQLITE3 QTCORE QTGUI ndnx database fs_watcher chronoshare',
          includes = "ndnx scheduler src executor gui fs-watcher",
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
        features = "qt4 cxx cxxprogram html_resources",
        defines = "WAF",
        source = bld.path.ant_glob(['gui/*.cpp', 'gui/*.cc', 'gui/images.qrc']),
        includes = "ndnx scheduler executor fs-watcher gui src adhoc server . ",
        use = "BOOST BOOST_FILESYSTEM BOOST_DATE_TIME SQLITE3 QTCORE QTGUI LOG4CXX fs_watcher ndnx database chronoshare http_server",

        html_resources = bld.path.find_dir ("gui/html").ant_glob([
                '**/*.js', '**/*.png', '**/*.css',
                '**/*.html', '**/*.gif', '**/*.ico'
                ]),
        )

    if Utils.unversioned_sys_platform () == "darwin":
        app_plist = '''<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist SYSTEM "file://localhost/System/Library/DTDs/PropertyList.dtd">
<plist version="0.9">
<dict>
    <key>CFBundlePackageType</key>
    <string>APPL</string>
    <key>CFBundleIconFile</key>
    <string>chronoshare.icns</string>
    <key>CFBundleGetInfoString</key>
    <string>Created by Waf</string>
    <key>CFBundleIdentifier</key>
    <string>edu.ucla.cs.irl.Chronoshare</string>
    <key>CFBundleSignature</key>
    <string>????</string>
    <key>NOTE</key>
    <string>THIS IS A GENERATED FILE, DO NOT MODIFY</string>
    <key>CFBundleExecutable</key>
    <string>%s</string>
    <key>LSUIElement</key>
    <string>1</string>
    <key>SUPublicDSAKeyFile</key>
    <string>dsa_pub.pem</string>
    <key>CFBundleIconFile</key>
    <string>chronoshare.icns</string>
</dict>
</plist>'''
        qt.mac_app = "ChronoShare.app"
        qt.mac_plist = app_plist % "ChronoShare"
        qt.mac_resources = 'chronoshare.icns'
        qt.use += " OSX_FOUNDATION OSX_COREWLAN adhoc"

        if bld.env['HAVE_SPARKLE']:
            qt.use += " OSX_SPARKLE"
            qt.source += ["osx/auto-update/sparkle-auto-update.mm"]
            qt.includes += " osx/auto-update"
            if bld.env['HAVE_LOCAL_SPARKLE']:
                qt.mac_frameworks = "osx/Frameworks/Sparkle.framework"

    if Utils.unversioned_sys_platform () == "linux":
        bld (
            features = "process_in",
            target = "ChronoShare.desktop",
            source = "ChronoShare.desktop.in",
            install_prefix = "${DATADIR}/applications",
            )
        bld.install_files ("${DATADIR}/applications", "ChronoShare.desktop")
        bld.install_files ("${DATADIR}/ChronoShare", "gui/images/chronoshare-big.png")

    cmdline = bld (
        target = "csd",
	features = "qt4 cxx cxxprogram",
	defines = "WAF",
	source = "cmd/csd.cc",
	includes = "ndnx scheduler executor gui fs-watcher src . ",
	use = "BOOST BOOST_FILESYSTEM BOOST_DATE_TIME SQLITE3 QTCORE QTGUI LOG4CXX fs_watcher ndnx database chronoshare"
	)

    dump_db = bld (
        target = "dump-db",
        features = "cxx cxxprogram",
	source = "cmd/dump-db.cc",
	includes = "ndnx scheduler executor gui fs-watcher src . ",
	use = "BOOST BOOST_FILESYSTEM BOOST_DATE_TIME SQLITE3 QTCORE LOG4CXX fs_watcher ndnx database chronoshare"
        )

from waflib import TaskGen
@TaskGen.extension('.mm')
def m_hook(self, node):
    """Alias .mm files to be compiled the same as .cc files, gcc/clang will do the right thing."""
    return self.create_compiled_task('cxx', node)

@TaskGen.extension('.js', '.png', '.css', '.html', '.gif', '.ico', '.in')
def sig_hook(self, node):
    node.sig=Utils.h_file (node.abspath())

@TaskGen.feature('process_in')
@TaskGen.after_method('process_source')
def create_process_in(self):
    dst = self.bld.path.find_or_declare (self.target)
    tsk = self.create_task ('process_in', self.source, dst)

class process_in(Task.Task):
    color='PINK'

    def run (self):
        self.outputs[0].write (Utils.subst_vars(self.inputs[0].read (), self.env))

@TaskGen.feature('html_resources')
@TaskGen.before_method('process_source')
def create_qrc_task(self):
    output = self.bld.path.find_or_declare ("gui/html.qrc")
    tsk = self.create_task('html_resources', self.html_resources, output)
    tsk.base_path = output.parent.get_src ()
    self.create_rcc_task (output.get_src ())

class html_resources(Task.Task):
    color='PINK'

    def __str__ (self):
        return "%s: Generating %s\n" % (self.__class__.__name__.replace('_task',''), self.outputs[0].nice_path ())

    def run (self):
        out = self.outputs[0]
        bld_out = out.get_bld ()
        src_out = out.get_src ()
        bld_out.write('<RCC>\n    <qresource prefix="/">\n')
        for f in self.inputs:
            bld_out.write ('        <file>%s</file>\n' % f.path_from (self.base_path), 'a')
        bld_out.write('    </qresource>\n</RCC>', 'a')

        src_out.write (bld_out.read(), 'w')
        return 0
