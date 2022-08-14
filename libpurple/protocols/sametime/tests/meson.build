foreach prog : ['im_mime']
	e = executable(
	    'test_sametime_' + prog, 'test_sametime_@0@.c'.format(prog),
	    link_with : [sametime_prpl],
	    dependencies : [libpurple_dep, gmime, glib])

	stenv = environment()
	stenv.set('G_TEST_SRCDIR', meson.current_source_dir())
	stenv.set('XDG_CONFIG_DIR', meson.current_build_dir() / 'config')

	test('sametime_' + prog, e,
	    env : stenv)
endforeach
