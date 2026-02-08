#! /usr/bin/env python
# encoding: utf-8
# a1batross, 2026

VERSION = '0.0'
APPNAME = 'noname'
top = '.'

def options(opt):
	opt.load('compiler_c clang_compilation_database')

def configure(conf):
	conf.load('compiler_c clang_compilation_database')
	conf.env.append_unique('CFLAGS', ['-fsigned-char', '-m32'])
	conf.env.append_unique('LINKFLAGS', ['-m32', '-Wl,--no-undefined'])

	if conf.env.cshlib_PATTERN[0:3] == 'lib':
		conf.env.cshlib_PATTERN = conf.env.cshlib_PATTERN[3:]

	conf.check(lib='X11')
	conf.check(lib='Xext')
	conf.check(lib='Xxf86dga')
	conf.check(lib='Xxf86vm')
	conf.check(lib='m')

def build(bld):
	botlibobjs = bld.path.ant_glob('botlib/*.c')
	bld.stlib(source = botlibobjs, target='bot', includes='botlib', defines='BOTLIB')

	q3objs = bld.path.ant_glob('client/*.c jpeg-6/*.c qcommon/*.c renderer/*.c server/*.c unix/*.c game/q_math.c game/q_shared.c')
	bld.program(source = q3objs, target='linuxquake3', includes='qcommon/', use='X11 XEXT XXF86DGA XXF86VM bot M')

	cgameobjs = bld.path.ant_glob('cgame/*.c game/q_math.c game/q_shared.c game/bg_misc.c game/bg_pmove.c game/bg_slidemove.c')
	bld.shlib(source = cgameobjs, target='baseq3/cgamei386', includes='qcommon', use='M')

	fgameobjs = bld.path.ant_glob('game/*.c')
	bld.shlib(source = fgameobjs, target='baseq3/gamei386', includes='qcommon', use='M')

	uiobjs = bld.path.ant_glob('q3_ui/*.c ui/ui_syscalls.c game/bg_misc.c game/q_math.c game/q_shared.c')
	bld.shlib(source = uiobjs, target='baseq3/uii386', includes='qcommon', use='M')

