TESTS = [
	'parser',
]

foreach prog : TESTS
	e = executable(
		'test_ircv3_' + prog, 'test_ircv3_@0@.c'.format(prog),
		dependencies : [libpurple_dep, glib],
		objects : ircv3_prpl.extract_all_objects())

	test('ircv3_' + prog, e)
endforeach
