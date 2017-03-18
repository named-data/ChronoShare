#!/usr/bin/env python
# -*- coding: utf-8 -*-
#
#
# Loosely based on original bash-version by Sebastian Schlingmann (based, again, on a OSX application bundler
# by Thomas Keller).
#

import sys, os, string, re, shutil, plistlib, tempfile, exceptions, datetime, tarfile
from subprocess import Popen, PIPE
from optparse import OptionParser

os.environ['PATH'] += ":/usr/local/opt/qt5/bin:/opt/qt5/5.8/clang_64/bin"

import platform

if platform.system () != 'Darwin':
  print "This script is indended to be run only on OSX platform"
  exit (1)

MIN_SUPPORTED_VERSION="10.12"

current_version = tuple(int(i) for i in platform.mac_ver()[0].split('.')[0:2])
min_supported_version = tuple(int(i) for i in MIN_SUPPORTED_VERSION.split('.')[0:2])

if current_version < min_supported_version:
  print "This script is indended to be run only on OSX >= %s platform" % MIN_SUPPORTED_VERSION
  exit (1)

options = None

def gitrev():
  return os.popen('git describe').read()[:-1]

def codesign(path):
  '''Call the codesign executable.'''

  if hasattr(path, 'isalpha'):
    path = (path,)

  for p in path:
    p = Popen(('codesign', '-vvvv', '--deep', '--force', '--sign', options.codesign, p))
    retval = p.wait()
    if retval != 0:
      return retval
  return 0

class AppBundle(object):

  def __init__(self, bundle, version, binary):
    shutil.copytree (src = binary, dst = bundle, symlinks = True)

    self.framework_path = ''
    self.handled_libs = {}
    self.bundle = bundle
    self.version = version
    self.infopath = os.path.join(os.path.abspath(bundle), 'Contents', 'Info.plist')
    self.infoplist = plistlib.readPlist(self.infopath)
    self.binary = os.path.join(os.path.abspath(bundle), 'Contents', 'MacOS', self.infoplist['CFBundleExecutable'])
    print ' * Preparing AppBundle'

  def is_system_lib(self, lib):
    '''
      Is the library a system library, meaning that we should not include it in our bundle?
    '''
    if lib.startswith('/System/Library/'):
      return True
    if lib.startswith('/usr/lib/'):
      return True

    return False

  def is_dylib(self, lib):
    '''
      Is the library a dylib?
    '''
    return lib.endswith('.dylib')

  def get_framework_base(self, fw):
    '''
      Extracts the base .framework bundle path from a library in an abitrary place in a framework.
    '''
    paths = fw.split('/')
    for i, str in enumerate(paths):
      if str.endswith('.framework'):
        return '/'.join(paths[:i+1])
    return None

  def is_framework(self, lib):
    '''
      Is the library a framework?
    '''
    return bool(self.get_framework_base(lib))

  def get_binary_libs(self, path):
    '''
      Get a list of libraries that we depend on.
    '''
    m = re.compile('^\t(.*)\ \(.*$')
    libs = Popen(['otool', '-L', path], stdout=PIPE).communicate()[0]
    libs = string.split(libs, '\n')
    ret = []
    bn = os.path.basename(path)
    for line in libs:
      g = m.match(line)
      if g is not None:
        lib = g.groups()[0]
        if lib != bn:
          ret.append(lib)
    return ret

  def handle_libs(self):
    '''
      Copy non-system libraries that we depend on into our bundle, and fix linker
      paths so they are relative to our bundle.
    '''
    print ' * Taking care of libraries'

    # Does our fwpath exist?
    fwpath = os.path.join(os.path.abspath(self.bundle), 'Contents', 'Frameworks')
    if not os.path.exists(fwpath):
      os.mkdir(fwpath)

    self.handle_binary_libs()

  def handle_binary_libs(self, macho=None, loader_path=None):
    '''
      Fix up dylib depends for a specific binary.
    '''
    # Does our fwpath exist already? If not, create it.
    if not self.framework_path:
      self.framework_path = self.bundle + '/Contents/Frameworks'
      if not os.path.exists(self.framework_path):
        os.mkdir(self.framework_path)
      else:
        shutil.rmtree(self.framework_path)
        os.mkdir(self.framework_path)

    # If we weren't explicitly told which binary to operate on, pick the
    # bundle's default executable from its property list.
    if macho is None:
      macho = os.path.abspath(self.binary)
    else:
      macho = os.path.abspath(macho)

    print "Processing [%s]" % macho

    libs = self.get_binary_libs(macho)

    for lib in libs:

      # Skip system libraries
      if self.is_system_lib(lib):
        continue

      if 'Qt' in lib:
        continue

      # Frameworks are 'special'.
      if self.is_framework(lib):
        fw_path = self.get_framework_base(lib)
        basename = os.path.basename(fw_path)
        name = basename.split('.framework')[0]
        rel = basename + '/' + name

        abs = self.framework_path + '/' + rel

        if not basename in self.handled_libs:
          dst = self.framework_path + '/' + basename
          print "COPY ", fw_path, dst
          shutil.copytree(fw_path, dst, symlinks=True)
          if name.startswith('Qt'):
            os.remove(dst + '/' + name + '.prl')
            os.remove(dst + '/Headers')
            shutil.rmtree(dst + '/Versions/Current/Headers')

          os.chmod(abs, 0755)
          os.system('install_name_tool -id "@executable_path/../Frameworks/%s" "%s"' % (rel, abs))
          self.handled_libs[basename] = True
          self.handle_binary_libs(abs)

        os.chmod(macho, 0755)
        # print 'install_name_tool -change "%s" "@executable_path/../Frameworks/%s" "%s"' % (lib, rel, macho)
        os.system('install_name_tool -change "%s" "@executable_path/../Frameworks/%s" "%s"' % (lib, rel, macho))

      # Regular dylibs
      else:
        basename = os.path.basename(lib)
        rel = basename

        if not basename in self.handled_libs:
          if lib.startswith('@loader_path'):
            copypath = lib.replace('@loader_path', loader_path)
          else:
            copypath = lib

          print "COPY ", copypath
          shutil.copy(copypath, self.framework_path  + '/' + basename)

          abs = self.framework_path + '/' + rel
          os.chmod(abs, 0755)
          os.system('install_name_tool -id "@executable_path/../Frameworks/%s" "%s"' % (rel, abs))
          self.handled_libs[basename] = True
          self.handle_binary_libs(abs, loader_path=os.path.dirname(lib))

        # print 'install_name_tool -change "%s" "@executable_path/../Frameworks/%s" "%s"' % (lib, rel, macho)
        os.chmod(macho, 0755)
        os.system('install_name_tool -change "%s" "@executable_path/../Frameworks/%s" "%s"' % (lib, rel, macho))

  def copy_resources(self, rsrcs):
    '''
      Copy needed resources into our bundle.
    '''
    print ' * Copying needed resources'
    rsrcpath = os.path.join(self.bundle, 'Contents', 'Resources')
    if not os.path.exists(rsrcpath):
      os.mkdir(rsrcpath)

    # Copy resources already in the bundle
    for rsrc in rsrcs:
      b = os.path.basename(rsrc)
      if os.path.isdir(rsrc):
        shutil.copytree(rsrc, os.path.join(rsrcpath, b), symlinks=True)
      elif os.path.isfile(rsrc):
        shutil.copy(rsrc, os.path.join(rsrcpath, b))

    return

  def copy_etc(self, rsrcs):
    '''
      Copy needed config files into our bundle.
    '''
    print ' * Copying needed config files'
    rsrcpath = os.path.join(self.bundle, 'Contents', 'etc', 'ndn')
    if not os.path.exists(rsrcpath):
      os.makedirs(rsrcpath)

    # Copy resources already in the bundle
    for rsrc in rsrcs:
      b = os.path.basename(rsrc)
      if os.path.isdir(rsrc):
        shutil.copytree(rsrc, os.path.join(rsrcpath, b), symlinks=True)
      elif os.path.isfile(rsrc):
        shutil.copy(rsrc, os.path.join(rsrcpath, b))

    return

  def copy_framework(self, framework):
    '''
      Copy frameworks
    '''
    print ' * Copying framework'
    rsrcpath = os.path.join(self.bundle, 'Contents', 'Frameworks', os.path.basename(framework))

    shutil.copytree(framework, rsrcpath, symlinks = True)

  def macdeployqt(self):
    Popen(['macdeployqt', self.bundle, '-qmldir=src', '-executable=%s' % self.binary]).communicate()

  def copy_ndn_deps(self, path):
    '''
      Copy over NDN dependencies (NFD and related apps)
    '''
    print ' * Copying NDN dependencies'

    src = os.path.join(path, 'bin')
    dst = os.path.join(self.bundle, 'Contents', 'Platform')
    shutil.copytree(src, dst, symlinks=False)

    for subdir, dirs, files in os.walk(dst):
      for file in files:
        abs = subdir + "/" + file
        self.handle_binary_libs(abs)

  def set_min_macosx_version(self, version):
    '''
      Set the minimum version of Mac OS X version that this App will run on.
    '''
    print ' * Setting minimum Mac OS X version to: %s' % (version)
    self.infoplist['LSMinimumSystemVersion'] = version

  def done(self):
    plistlib.writePlist(self.infoplist, self.infopath)
    print ' * Done!'
    print ''

class FolderObject(object):
  class Exception(exceptions.Exception):
    pass

  def __init__(self):
    self.tmp = tempfile.mkdtemp()

  def copy(self, src, dst='/'):
    '''
      Copy a file or directory into foler
    '''
    asrc = os.path.abspath(src)

    if dst[0] != '/':
      raise self.Exception

    # Determine destination
    if dst[-1] == '/':
      adst = os.path.abspath(self.tmp + '/' + dst + os.path.basename(src))
    else:
      adst = os.path.abspath(self.tmp + '/' + dst)

    if os.path.isdir(asrc):
      print ' * Copying directory: %s' % os.path.basename(asrc)
      shutil.copytree(asrc, adst, symlinks=True)
    elif os.path.isfile(asrc):
      print ' * Copying file: %s' % os.path.basename(asrc)
      shutil.copy(asrc, adst)

  def symlink(self, src, dst):
    '''
      Create a symlink inside the folder
    '''
    asrc = os.path.abspath(src)
    adst = self.tmp + '/' + dst
    print " * Creating symlink %s" % os.path.basename(asrc)
    os.symlink(asrc, adst)

  def mkdir(self, name):
    '''
      Create a directory inside the folder.
    '''
    print ' * Creating directory %s' % os.path.basename(name)
    adst = self.tmp + '/' + name
    os.makedirs(adst)

class DiskImage(FolderObject):

  def __init__(self, filename, volname):
    FolderObject.__init__(self)
    print ' * Preparing to create diskimage'
    self.filename = filename
    self.volname = volname

  def create(self):
    '''
      Create the disk image
    '''
    print ' * Creating disk image. Please wait...'
    if os.path.exists(self.filename):
      os.remove(self.filename)
      shutil.rmtree(self.filename, ignore_errors=True)
    p = Popen(['hdiutil', 'create',
              '-srcfolder', self.tmp,
              '-format', 'UDBZ',
              '-volname', self.volname,
              self.filename])

    retval = p.wait()
    print ' * Removing temporary directory.'
    shutil.rmtree(self.tmp)
    print ' * Done!'


if __name__ == '__main__':
  parser = OptionParser()
  parser.add_option('-r', '--release', dest='release', help='Build a release. This determines the version number of the release.')
  parser.add_option('-s', '--snapshot', dest='snapshot', help='Build a snapshot release. This determines the \'snapshot version\'.')
  parser.add_option('-g', '--git', dest='git', help='Build a snapshot release. Use the git revision number as the \'snapshot version\'.', action='store_true', default=False)
  parser.add_option('--no-dmg', dest='no_dmg', action='store_true', default=False, help='''Disable creation of DMG''')
  parser.add_option('--codesign', dest='codesign', help='Identity to use for code signing. (If not set, no code signing will occur)')

  options, args = parser.parse_args()

  # Release
  if options.release:
    ver = options.release
  # Snapshot
  elif options.snapshot or options.git:
    if not options.git:
      ver = options.snapshot
    else:
      ver = gitrev()
  else:
    print 'ERROR: Neither snapshot or release selected. Bailing.'
    parser.print_help ()
    sys.exit(1)

  # Do the finishing touches to our Application bundle before release
  shutil.rmtree('build/dist/ChronoShare.app', ignore_errors=True)
  a = AppBundle('build/dist/ChronoShare.app', ver, 'build/ChronoShare.app')
  # a.copy_resources(['qt.conf'])
  a.set_min_macosx_version('%s.0' % MIN_SUPPORTED_VERSION)
  a.handle_binary_libs()
  a.macdeployqt()
  a.copy_framework("osx/Frameworks/Sparkle.framework")
  a.done()

  # Sign our binaries, etc.
  if options.codesign:
    print ' * Signing binaries with identity `%s\'' % options.codesign
    codesign(a.bundle)
    print ''

  if not options.no_dmg:
    # Create diskimage
    title = "ChronoShare-%s" % ver
    fn = "build/%s.dmg" % title
    d = DiskImage(fn, title)
    d.symlink('/Applications', '/Applications')
    d.copy('build/dist/ChronoShare.app', '/ChronoShare.app')
    d.create()

    if options.codesign:
      print ' * Signing .dmg with identity `%s\'' % options.codesign
      codesign(fn)
      print ''

Popen('tail -n +3 RELEASE_NOTES.md | pandoc -f markdown -t html > build/release-notes-%s.html' % ver, shell=True).wait()
