if SOC == 'j721e' or SOC == 'j721s2' or SOC == 'j784s4' or SOC == 'am62a'
if not get_option('tests').disabled() and gst_check_dep.found()
  subdir('check')
endif
endif

if SOC == 'j721e' or SOC == 'j721s2' or SOC == 'j784s4' or SOC == 'am62a'
if not get_option('examples').disabled()
  subdir('examples')
endif
endif
