# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-
#!/usr/bin/env python
# encoding: utf-8
# Alexander Afanasyev, 2013

from waflib.Task import Task
from waflib.TaskGen import extension 
from waflib import Logs
from waflib import Node

"""
A simple tool to integrate zeroc ICE into your build system.

    def configure(conf):
        conf.load('compiler_cxx cxx ice_cxx')

    def build(bld):
        bld(
                features = 'cxx cxxprogram'
                source   = 'main.cpp file1.ice', 
                include  = '.',
                target   = 'executable') 

"""

class ice_cxx(Task):
        run_str = '${SLICE2CPP} --header-ext ice.h --source-ext ice.cc --output-dir ${SRC[0].parent.get_bld().abspath()}  ${SRC[0].abspath()}'
	color   = 'BLUE'
	ext_out = ['.ice.h', '.ice.cc']

@extension('.ice')
def process_protoc(self, node):
	cpp_node = node.change_ext('.ice.cc')
	hpp_node = node.change_ext('.ice.h')
	self.create_task('ice_cxx', node, [cpp_node, hpp_node])
	self.source.append(cpp_node)

	use = getattr(self, 'use', '')
	if not 'ICE' in use:
		self.use = self.to_list(use) + ['ICE']

ICE_PATHS = ['/usr', '/usr/local', '/opt/local', '/sw']

def options(opt):
        opt.add_option('--ice',action='store',dest='ICE_PATH',help='''path to ZeroC installation''')
        

def configure(conf):
        # conf.check_cfg(package="", uselib_store="PROTOBUF", args=['--cflags', '--libs'])
        if conf.options.ICE_PATH:
                conf.find_program('slice2cpp', var='SLICE2CPP', path_list="%s/bin" % conf.options.ICE_PATH, mandatory=True)
        else:
                conf.find_program('slice2cpp', var='SLICE2CPP', path_list=["%s/bin" % path for path in ICE_PATHS], mandatory=True)
        
        ICE_PATH = conf.env.SLICE2CPP[:-14]
        
	conf.env['LIB_ICE'] = ["Ice", "ZeroCIce", "IceUtil"]
	conf.env['INCLUDES_ICE']= '%s/include' % ICE_PATH
	conf.env['LIBPATH_ICE'] = '%s/lib' % ICE_PATH

