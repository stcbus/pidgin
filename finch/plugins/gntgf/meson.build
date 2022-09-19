if x11.found()
	library('gntgf', 'gntgf.c',
	    c_args : [
	        ncurses_header,
	        '-DG_LOG_USE_STRUCTURED',
	        '-DG_LOG_DOMAIN="Gnt-GF"',
	    ],
	    dependencies : [x11, libpurple_dep, libfinch_dep, ncurses, glib],
	    name_prefix : '',
	    install : true, install_dir : FINCH_PLUGINDIR)
endif

devenv.append('FINCH_PLUGIN_PATH', meson.current_build_dir())
