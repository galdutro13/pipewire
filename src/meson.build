
subdir('pipewire')
subdir('daemon')
subdir('tools')
subdir('modules')
subdir('media-session')
if not get_option('examples').disabled()
  subdir('examples')
endif
if not get_option('tests').disabled()
  subdir('tests')
endif

if gst_dep.length() != 0
  subdir('gst')
endif
