IRCV3_SOURCES = [
	'purpleircv3core.c',
	'purpleircv3protocol.c',
	'purpleircv3protocol.h',
]

if DYNAMIC_IRCV3
	ircv3_resources = gnome.compile_resources('ircv3resource',
		'resources/ircv3.gresource.xml',
		source_dir : 'resources',
		c_name : 'purple_ircv3')
	IRCV3_SOURCES += ircv3_resources

	ircv3_prpl = shared_library('ircv3', IRCV3_SOURCES,
	    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="Purple-IRCv3"'],
	    dependencies : [sasl, libpurple_dep, glib, gio, ws2_32],
	    install : true, install_dir : PURPLE_PLUGINDIR)

	devenv.append('PURPLE_PLUGIN_PATH', meson.current_build_dir())
endif
