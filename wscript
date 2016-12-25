# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-
VERSION='1.0'
APPNAME='ChronoShare'

from waflib import Logs, Utils, Task, TaskGen

def options(opt):
    opt.add_option('--with-tests', action='store_true', default=False, dest='with_tests',
                   help='''Build unit tests''')
    opt.add_option('--yes',action='store_true',default=False) # for autoconf/automake/make compatibility

    opt.add_option('--without-sqlite-locking', action='store_false', default=True,
                   dest='with_sqlite_locking',
                   help='''Disable filesystem locking in sqlite3 database '''
                        '''(use unix-dot locking mechanism instead). '''
                        '''This option may be necessary if home directory is hosted on NFS.''')

    if Utils.unversioned_sys_platform() == "darwin":
        opt.add_option('--with-auto-update', action='store_true', default=False, dest='autoupdate',
                       help='''(OSX) Download sparkle framework and enable autoupdate feature''')

    opt.load(['compiler_c', 'compiler_cxx', 'gnu_dirs', 'qt5'])
    opt.load(['default-compiler-flags',
              'osx-frameworks', 'boost', 'sqlite3', 'protoc', 'tinyxml',
              'coverage', 'sanitizers',
              'doxygen', 'sphinx_build'], tooldir=['.waf-tools'])

def configure(conf):
    conf.load(['compiler_c', 'compiler_cxx', 'gnu_dirs',
               'default-compiler-flags',
               'osx-frameworks', 'boost', 'sqlite3', 'protoc', 'tinyxml',
               'doxygen', 'sphinx_build'])

    if 'PKG_CONFIG_PATH' not in conf.environ:
        conf.environ['PKG_CONFIG_PATH'] = Utils.subst_vars('${LIBDIR}/pkgconfig', conf.env)

    conf.check_cfg(package='libndn-cxx', args=['--cflags', '--libs'],
               uselib_store='NDN_CXX')

    # add homebrew path, as qt5 is no longer linked
    conf.environ['PKG_CONFIG_PATH'] += ":/usr/local/opt/qt5/lib/pkgconfig:/opt/qt5/5.8/clang_64/lib/pkgconfig"
    conf.environ['PATH'] += ":/usr/local/opt/qt5/bin:/opt/qt5/5.8/clang_64/bin"

    conf.load('qt5')

    conf.define("CHRONOSHARE_VERSION", VERSION)

    conf.check_sqlite3(mandatory=True)
    if not conf.options.with_sqlite_locking:
        conf.define('DISABLE_SQLITE3_FS_LOCKING', 1)

    conf.check_tinyxml(path=conf.options.tinyxml_dir)

    conf.define("TRAY_ICON", "chronoshare-big.png")
    if Utils.unversioned_sys_platform() == "linux":
        conf.define("TRAY_ICON", "chronoshare-ubuntu.png")

    USED_BOOST_LIBS = ['system', 'filesystem', 'date_time', 'iostreams',
                       'regex', 'program_options', 'thread', 'log', 'log_setup']

    conf.env['WITH_TESTS'] = conf.options.with_tests
    if conf.env['WITH_TESTS']:
        USED_BOOST_LIBS += ['unit_test_framework']
        conf.define('HAVE_TESTS', 1)

    conf.check_boost(lib=USED_BOOST_LIBS, mt=True)
    if conf.env.BOOST_VERSION_NUMBER < 105400:
        Logs.error("Minimum required boost version is 1.54.0")
        Logs.error("Please upgrade your distribution or install custom boost libraries" +
                    " (https://redmine.named-data.net/projects/nfd/wiki/Boost_FAQ)")
        return

    # Loading "late" to prevent tests to be compiled with profiling flags
    conf.load('coverage')

    conf.load('sanitizers')

    conf.define('SYSCONFDIR', conf.env['SYSCONFDIR'])

    conf.write_config_header('core/chronoshare-config.hpp')

def build(bld):
    bld(name='core-objects',
        target='core-objects',
        features=['cxx'],
        source=bld.path.ant_glob('core/**/*.cpp'),
        use='NDN_CXX BOOST',
        includes='.',
        export_includes='.')

    # if Utils.unversioned_sys_platform() == 'darwin':
    #     bld(
    #         target='adhoc',
    #         mac_app = True,
    #         features=['cxx'],
    #         source='adhoc/adhoc-osx.mm'
    #         includes='. src',
    #         use='OSX_FOUNDATION OSX_COREWLAN',
    #     )
    Logs.error("Ad hoc network creation routines are temporary disabled")

    chornoshare = bld(
        target="chronoshare",
        features=['cxx'],
        source=bld.path.ant_glob(['src/*.proto',
                                  'src/db-helper.cpp',
                                  'src/sync-*.cpp',
                                  'src/file-state.cpp',
                                  'src/action-log.cpp',
                                  'src/object-*.cpp',
                                  ]),
        use='core-objects adhoc NDN_CXX BOOST TINYXML SQLITE3',
        includes="src",
        export_includes="src",
        )
    Logs.error("Most of Chronoshare source compilation is temporary disabled")

    # fs_watcher = bld(
    #     features=['qt5', 'cxx'],
    #     target='fs-watcher',
    #     defines='WAF',
    #     source=bld.path.ant_glob('fs-watcher/*.cpp'),
    #     use='chronoshare QT5CORE',
    #     includes='fs-watcher',
    #     export_includes='fs-watcher',
    #     )
    Logs.error("fs-watcher compilation is temporary disabled")

    http_server = bld(
          target = "http_server",
          features = "qt5 cxx",
          source = bld.path.ant_glob(['server/*.cpp']),
          includes = "server src .",
          use = 'BOOST QT5CORE',
          )

#     qt = bld(
#         target = "ChronoShare",
#         features = "qt5 cxx cxxprogram html_resources",
#         defines = "WAF",
#         source = bld.path.ant_glob(['gui/*.cpp', 'gui/images.qrc']),
#         includes = "fs-watcher gui src adhoc server . ",
#         use = "fs_watcher chronoshare http_server QT5CORE QT5GUI QT5WIDGETS",

#         html_resources = bld.path.find_dir("gui/html").ant_glob([
#                 '**/*.js', '**/*.png', '**/*.css',
#                 '**/*.html', '**/*.gif', '**/*.ico'
#                 ]),
#         )

#     if Utils.unversioned_sys_platform() == "darwin":
#         app_plist = '''<?xml version="1.0" encoding="UTF-8"?>
# <!DOCTYPE plist SYSTEM "file://localhost/System/Library/DTDs/PropertyList.dtd">
# <plist version="0.9">
# <dict>
#     <key>CFBundlePackageType</key>
#     <string>APPL</string>
#     <key>CFBundleIconFile</key>
#     <string>chronoshare.icns</string>
#     <key>CFBundleGetInfoString</key>
#     <string>Created by Waf</string>
#     <key>CFBundleIdentifier</key>
#     <string>edu.ucla.cs.irl.Chronoshare</string>
#     <key>CFBundleSignature</key>
#     <string>????</string>
#     <key>NOTE</key>
#     <string>THIS IS A GENERATED FILE, DO NOT MODIFY</string>
#     <key>CFBundleExecutable</key>
#     <string>%s</string>
#     <key>LSUIElement</key>
#     <string>1</string>
#     <key>SUPublicDSAKeyFile</key>
#     <string>dsa_pub.pem</string>
#     <key>CFBundleIconFile</key>
#     <string>chronoshare.icns</string>
# </dict>
# </plist>'''
#         qt.mac_app = "ChronoShare.app"
#         qt.mac_plist = app_plist % "ChronoShare"
#         qt.mac_resources = 'chronoshare.icns'
#         qt.use += " OSX_FOUNDATION OSX_COREWLAN adhoc"

#         if bld.env['HAVE_SPARKLE']:
#             qt.use += " OSX_SPARKLE"
#             qt.source += ["osx/auto-update/sparkle-auto-update.mm"]
#             qt.includes += " osx/auto-update"
#             if bld.env['HAVE_LOCAL_SPARKLE']:
#                 qt.mac_frameworks = "osx/Frameworks/Sparkle.framework"

#     if Utils.unversioned_sys_platform() == "linux":
#         bld(
#             features = "process_in",
#             target = "ChronoShare.desktop",
#             source = "ChronoShare.desktop.in",
#             install_prefix = "${DATADIR}/applications",
#             )
#         bld.install_files("${DATADIR}/applications", "ChronoShare.desktop")
#         bld.install_files("${DATADIR}/ChronoShare", "gui/images/chronoshare-big.png")
    Logs.error("ChronoShare app compilation is temporary disabled")

#     cmdline = bld(
#         target = "csd",
#         features = "qt5 cxx cxxprogram",
#         defines = "WAF",
#         source = bld.path.ant_glob(['cmd/csd.cpp']),
#         use = "fs_watcher chronoshare http_server QT5CORE",
#     )
    Logs.error("csd app compilation is temporary disabled")

#     dump_db = bld(
#         target = "dump-db",
#         features = "cxx cxxprogram",
#         source = bld.path.ant_glob(['cmd/dump-db.cpp']),
#         use = "fs_watcher chronoshare http_server QT5CORE",
#         )
    Logs.error("dump-db app compilation is temporary disabled")

    bld.recurse('tests');

from waflib import TaskGen
@TaskGen.extension('.mm')
def m_hook(self, node):
    """Alias .mm files to be compiled the same as .cpp files, gcc/clang will do the right thing."""
    return self.create_compiled_task('cxx', node)

@TaskGen.extension('.js', '.png', '.css', '.html', '.gif', '.ico', '.in')
def sig_hook(self, node):
    node.sig=Utils.h_file(node.abspath())

@TaskGen.feature('process_in')
@TaskGen.after_method('process_source')
def create_process_in(self):
    dst = self.bld.path.find_or_declare(self.target)
    tsk = self.create_task('process_in', self.source, dst)

class process_in(Task.Task):
    color='PINK'

    def run(self):
        self.outputs[0].write(Utils.subst_vars(self.inputs[0].read(), self.env))

@TaskGen.feature('html_resources')
@TaskGen.before_method('process_source')
def create_qrc_task(self):
    output = self.bld.path.find_or_declare("gui/html.qrc")
    tsk = self.create_task('html_resources', self.html_resources, output)
    tsk.base_path = output.parent.get_src()
    self.create_rcc_task(output.get_src())

class html_resources(Task.Task):
    color='PINK'

    def __str__(self):
        return "%s: Generating %s\n" % (self.__class__.__name__.replace('_task',''), self.outputs[0].path_from(self.outputs[0].ctx.launch_node()))

    def run(self):
        out = self.outputs[0]
        bld_out = out.get_bld()
        src_out = out.get_src()
        bld_out.write('<RCC>\n    <qresource prefix="/">\n')
        for f in self.inputs:
            bld_out.write('        <file>%s</file>\n' % f.path_from(self.base_path), 'a')
        bld_out.write('    </qresource>\n</RCC>', 'a')

        src_out.write(bld_out.read(), 'w')
        return 0
