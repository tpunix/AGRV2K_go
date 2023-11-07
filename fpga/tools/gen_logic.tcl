set sh_continue_on_error false
set sh_echo_on_source  true
set sh_quiet_on_source true
set cc_critical_as_fatal true
set ta_report_auto_constraints 1

set db_io_name_priority  true
set ip_pll_vco_lowpower true

set_seed_rand 10
if { ! [info exists LOGIC_DB] } {
  set LOGIC_DB logic_db
}
if { ! [info exists LOGIC_DEVICE] } {
  set LOGIC_DEVICE AGRV2KL100
}
if { ! [info exists LOGIC_MODULE] } {
  set LOGIC_MODULE top
}
if { ! [info exists LOGIC_DIR] } {
  set LOGIC_DIR .
}
if { ! [info exists LOGIC_VX] } {
  set LOGIC_VX logic.vx
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
if { ! [info exists DESIGN_ASF] } {
  set DESIGN_ASF ""
}
if { ! [info exists DESIGN_PRE] } {
  set DESIGN_PRE ""
}
if { ! [info exists DESIGN_POST] } {
  set DESIGN_POST ""
}
if { ! [info exists IP_ASF] } {
  set IP_ASF ""
}
if { ! [info exists IP_PRE] } {
  set IP_PRE ""
}
if { ! [info exists IP_POST] } {
  set IP_POST ""
}
if { ! [info exists LOGIC_BIN] } {
  set LOGIC_BIN logic.bin
}
if { ! [info exists LOGIC_COMPRESS] } {
  set LOGIC_COMPRESS false
}
if { ! [info exists IP_SDC] } {
  set IP_SDC ""
}

cd $LOGIC_DIR

if { $LOGIC_COMPRESS } {
  set_global_assignment -name ON_CHIP_BITSTREAM_DECOMPRESSION "ON"
} else {
  set_global_assignment -name ON_CHIP_BITSTREAM_DECOMPRESSION "OFF"
}

set logic_hx ${LOGIC_DESIGN}.hx
set hx_fp [open $logic_hx r]
set hsi_freq 0
set hse_freq 0
while { [gets $hx_fp line] >= 0 } {
  set words [split $line]
  if { [lindex $words 0] == "#define" } {
    if { [lindex $words 1] == "BOARD_HSI_FREQUENCY" } {
      set hsi_freq [lindex $words 2]
    }
    if { [lindex $words 1] == "BOARD_HSE_FREQUENCY" } {
      set hse_freq [lindex $words 2]
    }
    if { [lindex $words 1] == "USB0_MODE" } {
      set USB0_MODE [lindex $words 2]
    }
  }
}
close $hx_fp

alta::set_verbose_cmd false
set ::alta_work $LOGIC_DB

if { "$LOGIC_PRE" != "" } {
  alta::tcl_print "Processing $LOGIC_PRE ...\n"
  source "$LOGIC_PRE"
}
if { "$IP_PRE" != "" } {
  alta::tcl_print "Processing $IP_PRE ...\n"
  source "$IP_PRE"
}
if { "$DESIGN_PRE" != "" } {
  alta::tcl_print "Processing $DESIGN_PRE ...\n"
  source "$DESIGN_PRE"
}
load_architect -type $LOGIC_DEVICE
read_design -top $LOGIC_MODULE -ve "" -qsf "" $LOGIC_VX -hierachy 1

if { "$IP_SDC" != "" } {
  if { $hsi_freq != 0 } {
    set ::HSI_PERIOD [expr 1000000000.0/$hsi_freq]
  }
  if { $hse_freq != 0 } {
    set ::HSE_PERIOD [expr 1000000000.0/$hse_freq]
  }
  alta::tcl_print "Processing $IP_SDC ...\n"
  read_sdc "$IP_SDC"
} else {
  derive_pll_clocks -create_base_clocks
}

set_mode -skew basic -effort high -fitting auto -fitter full

if { "$LOGIC_ASF" != "" } {
  alta::tcl_print "Processing $LOGIC_ASF ...\n"
  source "$LOGIC_ASF"
}
if { "$IP_ASF" != "" } {
  alta::tcl_print "Processing $IP_ASF ...\n"
  source "$IP_ASF"
}
if { "$DESIGN_ASF" != "" } {
  alta::tcl_print "Processing $DESIGN_ASF ...\n"
  source "$DESIGN_ASF"
}

place_pseudo -user_io -place_io -place_pll -place_gclk -warn_io

set rt_max_iter "150 1 750 50  2 2"
place_and_route_design -quiet -retry 3

report_timing -verbose 2 -setup -file $::alta_work/setup.rpt.gz
report_timing -verbose 2 -setup -brief -file $::alta_work/setup_summary.rpt.gz
report_timing -verbose 2 -hold -file $::alta_work/hold.rpt.gz
report_timing -verbose 2 -hold -brief -file $::alta_work/hold_summary.rpt.gz

set ta_report_auto_constraints 0
report_timing -fmax -file $::alta_work/fmax.rpt
report_timing -xfer -file $::alta_work/xfer.rpt
set ta_report_auto_constraints 1

set ta_dump_uncovered 1
report_timing -verbose 1 -coverage >! $::alta_work/coverage.rpt.gz
unset ta_dump_uncovered

set logic_routed ${LOGIC_DESIGN}_routed.v
write_routed_design $logic_routed

bitgen normal -bin $LOGIC_BIN

if { "$LOGIC_POST" != "" } {
  alta::tcl_print "Processing $LOGIC_POST ...\n"
  source "$LOGIC_POST"
}
if { "$IP_POST" != "" } {
  alta::tcl_print "Processing $IP_POST ...\n"
  source "$IP_POST"
}
if { "$DESIGN_POST" != "" } {
  alta::tcl_print "Processing $DESIGN_POST ...\n"
  source "$DESIGN_POST"
}

exit
