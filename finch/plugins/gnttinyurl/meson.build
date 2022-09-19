library('gnttinyurl', 'gnttinyurl.c',
    c_args : ['-DG_LOG_USE_STRUCTURED', '-DG_LOG_DOMAIN="Gnt-TinyURL"'],
    dependencies : [libpurple_dep, libfinch_dep, libsoup, glib],
    name_prefix : '',
    install : true, install_dir : FINCH_PLUGINDIR)

devenv.append('FINCH_PLUGIN_PATH', meson.current_build_dir())
