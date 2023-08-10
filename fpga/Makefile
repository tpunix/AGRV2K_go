
# 指定Supra的安装目录。可以用AgRV_pio里面带的supra，也可以用Supra安装包。

#SUPRA_DIR = /e/AGRV/AgRV_pio/packages/tool-agrv_logic
SUPRA_DIR = /e//AGRV/Supra

AF       = $(SUPRA_DIR)/bin/af.exe
OPENOCD  = $(SUPRA_DIR)/openocd/bin/openocd.exe -c "set CONNECT_UNDER_RESET 1" -f $(SUPRA_DIR)/openocd//agrv2k.cfg
GEN_VLOG = tools/gen_vlog

DEVICE   = AGRV2KL100


proj: prevlog preip


# 第一步: 根据ve文件，生成顶层模块以及user模板。你可以在生成的ulogic.v基础上进行开发。
prevlog:
	$(GEN_VLOG) -s -d $(DEVICE) -c ulogic/top.hx -m ulogic/ulogic.v top.ve ulogic/top.v


# 第二步: 生成quartus工程. 需要编译生成netlist.
preip:
	$(AF) \
		-L ulogic/preip_log.txt \
		-X "set LOGIC_DEVICE $(DEVICE)" \
		-X "set LOGIC_DESIGN top" \
		-X "set LOGIC_DIR ulogic" \
		-X "set LOGIC_VV  top.v" \
		-X "set LOGIC_ASF  {top.asf}" \
		-X "set LOGIC_PRE  {}" \
		-X "set LOGIC_POST {}" \
		-X "set LOGIC_FORCE {false}" \
		-X "set IP_VV {ulogic.v}" \
		-X "set ORIGINAL_PIN {-}" \
		-F tools/pre_logic.tcl



# 第三步: 调用Supra工具，编译转换netlist文件，生成烧写文件。
genbin:
	cd ulogic; $(AF) -B --batch --mode QUARTUS -X "set QUARTUS_SDC true" -X "set FITTING Auto" -X "set FITTER full" -X "set EFFORT high" -X "set HOLDX default" -X "set SKEW basic"


# 第四步: 调用OpenOCD写入。
dl:
	$(OPENOCD) -c "agrv write_fpga_config ulogic/top.bin" -c "reset" -c "exit"

