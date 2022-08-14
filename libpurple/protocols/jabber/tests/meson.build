foreach prog : ['caps', 'digest_md5', 'scram', 'jutil']
	e = executable(
	    'test_jabber_' + prog, 'test_jabber_@0@.c'.format(prog),
	    link_with : [jabber_prpl],
	    dependencies : [libxml, libpurple_dep, libsoup, glib])

	jabberenv = environment()
	jabberenv.set('XDG_CONFIG_DIR', meson.current_build_dir() / 'config')

	test('jabber_' + prog, e,
	    env: jabberenv)
endforeach
