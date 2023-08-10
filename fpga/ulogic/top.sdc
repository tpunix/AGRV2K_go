# pio_begin
if { ! [info exists ::HSI_PERIOD] } {
  set ::HSI_PERIOD 100.0
}
create_clock -name PIN_HSI -period $::HSI_PERIOD [get_ports PIN_HSI]
set_clock_groups -asynchronous -group PIN_HSI
if { ! [info exists ::HSE_PERIOD] } {
  set ::HSE_PERIOD 125.0
}
create_clock -name PIN_HSE -period $::HSE_PERIOD [get_ports PIN_HSE]
set_clock_groups -asynchronous -group PIN_HSE
derive_pll_clocks -create_base_clocks
# pio_end


set SYS_CLK [get_clocks pll_inst|*clk*0*]
set BUS_CLK [get_clocks pll_inst|*clk*3*]


set_false_path -from $SYS_CLK -to $BUS_CLK
set_false_path -from $BUS_CLK -to $SYS_CLK

