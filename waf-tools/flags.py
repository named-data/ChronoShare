#! /usr/bin/env python
# encoding: utf-8

from waflib import Configure

@Configure.conf
def add_supported_cflags(self, cflags):
    """
    Check which cflags are supported by compiler and add them to env.CFLAGS variable
    """
    self.start_msg('Checking allowed flags for c compiler')

    supportedFlags = []
    for flag in cflags:
        if self.check_cc (cflags=[flag], mandatory=False):
            supportedFlags += [flag]

    self.end_msg (' '.join (supportedFlags))
    self.env.CFLAGS += supportedFlags

def configure(conf):
    conf.load ('gnu_dirs')
    
    if conf.options.debug:
        conf.define ('_DEBUG', 1)
        conf.add_supported_cflags (cflags = ['-O0',
                                             '-Wall',
                                             '-Wno-unused-variable',
                                             '-g3',
                                             '-Wno-unused-private-field', # only clang supports
                                             '-fcolor-diagnostics',       # only clang supports
                                             '-Qunused-arguments'         # only clang supports
                                             ])
    else:
        conf.add_supported_cflags (cflags = ['-O3', '-g'])

def options(opt):
    opt.load ('gnu_dirs')
    opt.add_option('--debug',action='store_true',default=False,dest='debug',help='''debugging mode''')
