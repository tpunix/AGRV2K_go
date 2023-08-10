set sh_continue_on_error false
set sh_echo_on_source  true
set sh_quiet_on_source true
set cc_critical_as_fatal true

set_seed_rand 10
if { ! [info exists LOGIC_DEVICE] } {
  set LOGIC_DEVICE AGRV2KL100
}
if { ! [info exists LOGIC_DESIGN] } {
  set LOGIC_DESIGN top
}
if { ! [info exists LOGIC_MODULE] } {
  set LOGIC_MODULE top
}
if { ! [info exists IP_INSTALL_DIR] } {
  set IP_INSTALL_DIR ""
}
if { ! [info exists LOGIC_DIR] } {
  set LOGIC_DIR .
}
if { ! [info exists LOGIC_VV] } {
  set LOGIC_VV "${LOGIC_DESIGN}.v"
}
if { ! [info exists IP_VV] } {
  set IP_VV ""
}
if { ! [info exists LOGIC_ASF] } {
  set LOGIC_ASF ""
}
if { ! [info exists LOGIC_PRE] } {
  set LOGIC_PRE ""
}
if { ! [info exists LOGIC_POST] } {
  set LOGIC_POST ""
}
if { ! [info exists LOGIC_FORCE] } {
  set LOGIC_FORCE false
}
if { ! [info exists LOGIC_COMPRESS] } {
  set LOGIC_COMPRESS false
}

cd $LOGIC_DIR

alta::set_verbose_cmd false
set logic_qsf ${LOGIC_DESIGN}.qsf
set skip_setup false
if { [file exists $logic_qsf] } {
  if { $LOGIC_FORCE } {
    alta::tcl_info "Overwrite existing LOGIC preparation files in $LOGIC_DIR"
  } else {
    alta::tcl_warn "Files for LOGIC preparation already exist in $LOGIC_DIR, will not overwrite"
    set skip_setup true
  }
}
set logic_ip false
if { $IP_INSTALL_DIR != "" } {
  set logic_ip true
}

set ETC_DIR [file join [alta::prog_home] "etc"]
set IP_FILES ""
set VERILOG_FILES $LOGIC_VV
if { $IP_VV != "" } {
  set VERILOG_FILES "$VERILOG_FILES $IP_VV"
}
set VQM_FILES ""
set VHDL_FILES ""
set AF_QUARTUS_TEMPL [file join $ETC_DIR "af_quartus.tcl"]
set AF_QUARTUS "af_quartus.tcl"
set AF_IP_TEMPL [file join $ETC_DIR "af_ip.tcl"]
set AF_IP "af_ip.tcl"
set AF_MAP_TEMPL ""
set AF_MAP ""
set AF_RUN_TEMPL [file join $ETC_DIR "af_run.tcl"]
set AF_RUN "af_run.tcl"
set AF_BATCH_TEMPL ""
set AF_BATCH ""
set VE_FILE ""

if { ! [info exists ORIGINAL_DIR] } {
  set ORIGINAL_DIR ""
}
if { ! [info exists ORIGINAL_OUTPUT] } {
  set ORIGINAL_OUTPUT ""
}
if { ! [info exists ORIGINAL_QSF] } {
  set ORIGINAL_QSF ""
}
if { ! [info exists ORIGINAL_PIN] } {
  set ORIGINAL_PIN ""
}

set GCLK_CNT -1; # Allow an extra gclk for GCLKSW
set USE_DESIGN_TEMPL false

set logic_hx ${LOGIC_DESIGN}.hx
set hx_fp [open $logic_hx r]
set hsi_freq 0
set hse_freq 0
set sys_freq 0
set bus_freq 0
while { [gets $hx_fp line] >= 0 } {
  set words [split $line]
  if { [lindex $words 0] == "#define" } {
    if { [lindex $words 1] == "BOARD_HSI_FREQUENCY" } {
      set hsi_freq [lindex $words 2]
    } elseif { [lindex $words 1] == "BOARD_HSE_FREQUENCY" } {
      set hse_freq [lindex $words 2]
    } elseif { [lindex $words 1] == "BOARD_PLL_FREQUENCY" } {
      set sys_freq [lindex $words 2]
    } elseif { [lindex $words 1] == "BOARD_BUS_FREQUENCY" } {
      set bus_freq [lindex $words 2]
    }
    if { [lindex $words 1] == "USB0_MODE" } {
      set USB0_MODE [lindex $words 2]
    }
  }
}
close $hx_fp

if { ! $logic_ip } {
  set sdc_file ${LOGIC_DESIGN}.sdc
  set sdc_ip ""
} else {
  set sdc_file ${LOGIC_DESIGN}_.sdc
  set sdc_ip ${LOGIC_DESIGN}.sdc
}

if { ! $skip_setup } {
  if { ! [file exists $sdc_file] } {
    set sdc_fp [open $sdc_file w]
    puts $sdc_fp "# pio_begin"
    if { $hsi_freq != 0 } {
      set hsi_period [expr 1000000000.0/$hsi_freq]
      puts $sdc_fp "if { ! \[info exists ::HSI_PERIOD\] } {"
      puts $sdc_fp "  set ::HSI_PERIOD $hsi_period"
      puts $sdc_fp "}"
      puts $sdc_fp "create_clock -name PIN_HSI -period \$::HSI_PERIOD \[get_ports PIN_HSI\]"
      puts $sdc_fp "set_clock_groups -asynchronous -group PIN_HSI"
    }
    if { $hse_freq != 0 } {
      set hse_period [expr 1000000000.0/$hse_freq]
      puts $sdc_fp "if { ! \[info exists ::HSE_PERIOD\] } {"
      puts $sdc_fp "  set ::HSE_PERIOD $hse_period"
      puts $sdc_fp "}"
      puts $sdc_fp "create_clock -name PIN_HSE -period \$::HSE_PERIOD \[get_ports PIN_HSE\]"
      puts $sdc_fp "set_clock_groups -asynchronous -group PIN_HSE"
    }
    puts $sdc_fp "derive_pll_clocks -create_base_clocks"
    puts $sdc_fp "# pio_end"
    close $sdc_fp
  }

  if { $sdc_ip != "" && ! [file exists $sdc_ip] } {
    set sdc_fp [open $sdc_ip w]
    puts $sdc_fp "# pio_begin"
    if { $sys_freq != 0 } {
      set sys_period [expr 1000000000.0/$sys_freq]
      puts $sdc_fp "create_clock -name sys_clock -period $sys_period \[get_ports sys_clock\]"
    }
    if { $bus_freq != 0 } {
      set bus_period [expr 1000000000.0/$bus_freq]
      puts $sdc_fp "create_clock -name bus_clock -period $bus_period \[get_ports bus_clock\]"
    }
    puts $sdc_fp "# pio_end"
    close $sdc_fp
  }

  load_architect -no_route -type $LOGIC_DEVICE

  alta::setupRun ${LOGIC_DESIGN} ${LOGIC_MODULE} \
                 "${IP_FILES}" \
                 "${VERILOG_FILES}" \
                 "${VQM_FILES}" \
                 "${VHDL_FILES}"\
                 "${AF_QUARTUS_TEMPL}" "${AF_QUARTUS}"\
                 "${AF_IP_TEMPL}" "${AF_IP}" \
                 "${AF_MAP_TEMPL}" "${AF_MAP}" \
                 "${AF_RUN_TEMPL}" "${AF_RUN}" \
                 "${AF_BATCH_TEMPL}" "${AF_BATCH}" \
                 "${VE_FILE}" \
                 "." "${ORIGINAL_DIR}" "${ORIGINAL_OUTPUT}"\
                 "${ORIGINAL_QSF}" "${ORIGINAL_PIN}" \
                 "${GCLK_CNT}" "${USE_DESIGN_TEMPL}"

  if { $logic_ip } {
    set qsf_fp [open $logic_qsf]
    set qsf_lines {}
    set is_pio false
    while { [gets $qsf_fp line] >= 0 } {
      if { [string first "pio_begin" $line] >= 0 } {
        set is_pio true
      } elseif { [string first "pio_end" $line] >= 0 } {
        set is_pio false
      } elseif { ! $is_pio } {
        lappend qsf_lines $line
      }
    }
    close $qsf_fp
    set qsf_fp [open $logic_qsf w]
    puts $qsf_fp "# pio_begin  >>>>>> DO NOT MODIFY THIS SECTION! >>>>>>"
    puts $qsf_fp "set_instance_assignment -name VIRTUAL_PIN ON -to *"
    puts $qsf_fp "# pio_end    <<<<<< DO NOT MODIFY THIS SECTION! <<<<<<"
    foreach line $qsf_lines {
      puts $qsf_fp $line
    }
    close $qsf_fp

    set af_run af_run.tcl
    set run_fp [open $af_run]
    set run_lines {}
    set is_pio false
    while { [gets $run_fp line] >= 0 } {
      if { [string first "pio_begin" $line] >= 0 } {
        set is_pio true
      } elseif { [string first "pio_end" $line] >= 0 } {
        set is_pio false
      } elseif { ! $is_pio } {
        lappend run_lines $line
      }
    }
    close $run_fp
    set run_fp [open $af_run w]
    puts $run_fp "# pio_begin  >>>>>> DO NOT MODIFY THIS SECTION! >>>>>>"
    puts $run_fp "set FLOW PACK"
    puts $run_fp "# pio_end    <<<<<< DO NOT MODIFY THIS SECTION! <<<<<<"
    foreach line $run_lines {
      puts $run_fp $line
    }
    close $run_fp
  }

  if { true } {
    set supra_proj ${LOGIC_DESIGN}.proj
    set proj_fp [open $supra_proj w]
    puts $proj_fp {[GuiMigrateSetupPage]}
    puts $proj_fp "design=$LOGIC_DESIGN"
    puts $proj_fp "device=$LOGIC_DEVICE"
    puts $proj_fp ""
    puts $proj_fp {[GuiMigrateRunPage]}
    puts $proj_fp "fitting=1"
    puts $proj_fp "fitter=5"
    puts $proj_fp "effort=2"
    puts $proj_fp "skew=2"
    if { $logic_ip } {
      puts $proj_fp "flow=0"
    }
    close $proj_fp
  }
}

if { true } {
  set pre_asf ${LOGIC_DESIGN}.pre.asf
  set pre_fp [open $pre_asf r];
  set pre_lines {}
  set is_pio false
  while { [gets $pre_fp line] >= 0 } {
    if { [string first "db_io_name_priority" $line] >= 0 ||
         [string first "pio_begin" $line] >= 0 } {
      set is_pio true
    } elseif { [string first "pio_end" $line] >= 0 } {
      set is_pio false
    } elseif { ! $is_pio } {
      lappend pre_lines $line
    }
  }
  close $pre_fp
  set pre_fp [open $pre_asf w]
  foreach line $pre_lines {
    puts $pre_fp $line
  }
  puts $pre_fp "# pio_begin  >>>>>> DO NOT MODIFY THIS SECTION! >>>>>>"
  if { "$LOGIC_PRE" != "" } {
    set logic_fp [open $LOGIC_PRE r]; set logic_data [read $logic_fp]; close $logic_fp
    puts -nonewline $pre_fp $logic_data
  }
  if { [info exists USB0_MODE] } {
    puts $pre_fp "set USB0_MODE $USB0_MODE"
  }
  puts $pre_fp "set db_io_name_priority true"
  puts $pre_fp "set ip_pll_vco_lowpower true"
  if { $LOGIC_COMPRESS } {
    puts $pre_fp "set_global_assignment -name ON_CHIP_BITSTREAM_DECOMPRESSION \"ON\""
  } else {
    puts $pre_fp "set_global_assignment -name ON_CHIP_BITSTREAM_DECOMPRESSION \"OFF\""
  }
  puts $pre_fp "# pio_end    <<<<<< DO NOT MODIFY THIS SECTION! <<<<<<"
  close $pre_fp
}

if { true } {
  set asf_asf ${LOGIC_DESIGN}.asf
  set asf_fp [open $asf_asf r];
  set asf_lines {}
  set is_pio false
  while { [gets $asf_fp line] >= 0 } {
    if { [string first "db_io_name_priority" $line] >= 0 ||
         [string first "pio_begin" $line] >= 0 } {
      set is_pio true
    } elseif { [string first "pio_end" $line] >= 0 } {
      set is_pio false
    } elseif { ! $is_pio } {
      lappend asf_lines $line
    }
  }
  close $asf_fp
  set asf_fp [open $asf_asf w]
  foreach line $asf_lines {
    puts $asf_fp $line
  }
  puts $asf_fp "# pio_begin  >>>>>> DO NOT MODIFY THIS SECTION! >>>>>>"
  if { "$LOGIC_ASF" != "" } {
    set logic_fp [open $LOGIC_ASF r]; set logic_data [read $logic_fp]; close $logic_fp
    puts -nonewline $asf_fp $logic_data
  }
  puts $asf_fp "# pio_end    <<<<<< DO NOT MODIFY THIS SECTION! <<<<<<"
  close $asf_fp
}

if { true } {
  set post_asf ${LOGIC_DESIGN}.post.asf
  set post_fp [open $post_asf r];
  set post_lines {}
  set is_pio false
  while { [gets $post_fp line] >= 0 } {
    if { [string first "pio_begin" $line] >= 0 } {
      set is_pio true
    } elseif { [string first "pio_end" $line] >= 0 } {
      set is_pio false
    } elseif { ! $is_pio } {
      lappend post_lines $line
    }
  }
  close $post_fp
  set post_fp [open $post_asf w]
  foreach line $post_lines {
    puts $post_fp $line
  }
  puts $post_fp "# pio_begin  >>>>>> DO NOT MODIFY THIS SECTION! >>>>>>"
  if { "$LOGIC_POST" != "" } {
    set logic_fp [open $LOGIC_POST r]; set logic_data [read $logic_fp]; close $logic_fp
    puts -nonewline $post_fp $logic_data
  }
  if { $logic_ip } {
    puts $post_fp "file mkdir $IP_INSTALL_DIR"
    puts $post_fp "if { ! \[file exists $IP_INSTALL_DIR/${LOGIC_DESIGN}.sdc\] } {"
    puts $post_fp "  file copy -force ./${LOGIC_DESIGN}_.sdc $IP_INSTALL_DIR/${LOGIC_DESIGN}.sdc"
    puts $post_fp "}"
    puts $post_fp "file copy -force ./${LOGIC_DESIGN}_.ve $IP_INSTALL_DIR/${LOGIC_DESIGN}.ve"
    puts $post_fp "file copy -force ./alta_db/packed.vx $IP_INSTALL_DIR/${LOGIC_DESIGN}.vx"
  }
  puts $post_fp "# pio_end    <<<<<< DO NOT MODIFY THIS SECTION! <<<<<<"
  close $post_fp
}

exit
