# -*- Mode: python; py-indent-offset: 4; indent-tabs-mode: nil; coding: utf-8; -*-

#! /usr/bin/env python
# encoding: utf-8

'''

When using this tool, the wscript will look like:

def options(opt):
   opt.tool_options("protobuf", tooldir=["waf-tools"])

def configure(conf):
   conf.load("compiler_cxx protobuf")

def build(bld):
   bld(source="main.cpp", target="app", use="PROTOBUF")

Options are generated, in order to specify the location of protobuf includes/libraries.

'''

from waflib import Utils, TaskGen, Task, Logs

from waflib import Utils,Logs,Errors
from waflib.Configure import conf

def options(opt):
    pass

def configure(conf):
    """
    """
    conf.check_cfg(package='protobuf', args=['--cflags', '--libs'], uselib_store='PROTOBUF', mandatory=True)

    conf.find_program ('protoc', var='PROTOC', path_list = conf.env['PATH'], mandatory = True)

@TaskGen.extension('.proto')
def add_proto(self, node):
    """
    Compile PROTOBUF protocol specifications
    """
    prototask = self.create_task ("protobuf", node, node.change_ext (".pb"))
    try:
        self.compiled_tasks.append (prototask)
    except AttributeError:
        self.compiled_tasks = [prototask]

class protobuf(Task.Task):
    """
    Task for compiling PROTOBUF protocol specifications
    """
    run_str = '${PROTOC} --cpp_out ${TGT[0].parent.abspath()} --proto_path ${SRC[0].parent.abspath()} ${SRC[0].abspath()} -o${TGT[0].abspath()}'
    color   = 'BLUE'

@TaskGen.feature('cxxshlib')
@TaskGen.before_method('process_source')
def dynamic_post(self):
    if not getattr(self, 'dynamic_source', None):
        return
    self.source = Utils.to_list(self.source)

    src = self.bld.path.get_bld().ant_glob (Utils.to_list(self.dynamic_source))

    for cc in src:
        # Signature for the source
        cc.sig = Utils.h_file (cc.abspath())
        # Signature for the header
        h = cc.change_ext (".h")
        h.sig = Utils.h_file (h.abspath())

    self.source.extend (src)
