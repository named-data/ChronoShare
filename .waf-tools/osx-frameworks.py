#! /usr/bin/env python
# encoding: utf-8

from waflib import Logs, Utils
from waflib.Configure import conf

OSX_SECURITY_CODE='''
#include <CoreFoundation/CoreFoundation.h>
#include <Security/Security.h>
#include <Security/SecRandom.h>
#include <CoreServices/CoreServices.h>
#include <Security/SecDigestTransform.h>

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    return 0;
}
'''

@conf
def configure(conf):
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
