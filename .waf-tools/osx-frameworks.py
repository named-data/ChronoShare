#! /usr/bin/env python
# encoding: utf-8

from waflib import Logs, Utils
from waflib.Configure import conf

OSX_SECURITY_CODE='''
#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <Sparkle/Sparkle.h>
#import <CoreWLAN/CWInterface.h>
#import <CoreWLAN/CoreWLAN.h>
#import <CoreWLAN/CoreWLANConstants.h>
#import <CoreWLAN/CoreWLANTypes.h>
int main() { }
'''

@conf
def configure(conf):
    if Utils.unversioned_sys_platform () == "darwin":
        conf.check_cxx(framework_name='Foundation', uselib_store='OSX_FOUNDATION', mandatory=False, compile_filename='test.mm')
        conf.check_cxx(framework_name='AppKit',     uselib_store='OSX_APPKIT',     mandatory=False, compile_filename='test.mm')
        conf.check_cxx(framework_name='CoreWLAN',   uselib_store='OSX_COREWLAN',   define_name='HAVE_COREWLAN',
                       use="OSX_FOUNDATION", mandatory=False, compile_filename='test.mm')

        def check_sparkle(**kwargs):
          conf.check_cxx(framework_name="Sparkle", header_name=["Foundation/Foundation.h", "AppKit/AppKit.h"],
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

                    urllib.urlretrieve ("https://github.com/sparkle-project/Sparkle/releases/download/1.17.0/Sparkle-1.17.0.tar.bz2", "build/Sparkle.tar.bz2")
                    if os.path.exists('build/Sparkle.tar.bz2'):
                        try:
                            subprocess.check_call(['mkdir', 'build/Sparkle'])
                            subprocess.check_call(['tar', 'xjf', 'build/Sparkle.tar.bz2', '-C', 'build/Sparkle'])
                            os.remove("build/Sparkle.tar.bz2")
                            if not os.path.exists("osx/Frameworks"):
                                os.mkdir ("osx/Frameworks")
                            os.rename("build/Sparkle/Sparkle.framework", "osx/Frameworks/Sparkle.framework")

                            check_sparkle(cxxflags="-F%s/osx/Frameworks/" % conf.path.abspath(),
                                          linkflags="-F%s/osx/Frameworks/" % conf.path.abspath())
                            conf.env.HAVE_LOCAL_SPARKLE = 1
                        except subprocess.CalledProcessError as e:
                            conf.fatal("Cannot find Sparkle framework. Auto download failed: '%s' returned %s" % (' '.join(e.cmd), e.returncode))
                        except:
                            conf.fatal("Unknown Error happened when auto downloading Sparkle framework")

        conf.env['LDFLAGS_OSX_SPARKLE'] += ['-Wl,-rpath,@loader_path/../Frameworks']
        if conf.is_defined('HAVE_SPARKLE'):
            conf.env.HAVE_SPARKLE = 1 # small cheat for wscript
