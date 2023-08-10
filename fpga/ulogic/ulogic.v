
///////////////////////////////////////////////////////
// Module: user logic for AGRV2KL100                 //
///////////////////////////////////////////////////////

module ulogic (
  // direct pins
  output        soc_led,
  input         soc_s2,
  output        soc_spi_clk,
  output        soc_spi_csn,
  inout			soc_spi_io0,
  inout			soc_spi_io1,
  inout			soc_spi_io2,
  inout			soc_spi_io3,
  // from mcu func
  output        ip_pti_in,
  input         ip_pto_out_data,
  input         ip_pto_out_en,
  // from mcu system
  input         sys_clock,
  input         bus_clock,
  input         resetn,
  input         stop,
  // AHB slave
  input  [1:0]  mem_ahb_htrans,
  input         mem_ahb_hready,
  input         mem_ahb_hwrite,
  input  [31:0] mem_ahb_haddr,
  input  [2:0]  mem_ahb_hsize,
  input  [2:0]  mem_ahb_hburst,
  input  [31:0] mem_ahb_hwdata,
  output reg    mem_ahb_hreadyout,
  output        mem_ahb_hresp,
  output [31:0] mem_ahb_hrdata,
  // AHB master
  output        slave_ahb_hsel,
  output tri1   slave_ahb_hready,
  input         slave_ahb_hreadyout,
  output [1:0]  slave_ahb_htrans,
  output [2:0]  slave_ahb_hsize,
  output [2:0]  slave_ahb_hburst,
  output        slave_ahb_hwrite,
  output [31:0] slave_ahb_haddr,
  output [31:0] slave_ahb_hwdata,
  input         slave_ahb_hresp,
  input  [31:0] slave_ahb_hrdata,
  // DMA
  output [3:0]  ext_dma_DMACBREQ,
  output [3:0]  ext_dma_DMACLBREQ,
  output [3:0]  ext_dma_DMACSREQ,
  output [3:0]  ext_dma_DMACLSREQ,
  input  [3:0]  ext_dma_DMACCLR,
  input  [3:0]  ext_dma_DMACTC,
  output [3:0]  local_int
);

///////////////////////////////////////////////////////
// Logics                                            //
///////////////////////////////////////////////////////


assign ip_pti_in = soc_s2;

assign soc_led = soc_s2 | (ip_pto_out_en & ip_pto_out_data);



///////////////////////////////////////////////////////
// AHB Master                                        //
///////////////////////////////////////////////////////


	wire aclk = sys_clock;
	wire bclk = bus_clock;

	// 高速AHB到低速BUSCLK的握手信号
	reg ahb_start;
	reg ahb_ack;
	reg ahb_trans_d;

	always @(posedge aclk)
	begin
		ahb_trans_d <= mem_ahb_htrans[1];
	end

	always @(negedge resetn or posedge aclk)
	begin
		if(resetn==0) begin
			ahb_start <= 1'b0;
		end else begin
			if(ahb_ack)
				ahb_start <= 1'b0;
			else if(ahb_trans_d==0 && mem_ahb_htrans[1]==1)
				ahb_start <= 1'b1;
		end
	end

	always @(posedge bclk)
	begin
		ahb_ack <= ahb_start;
	end


	//reg mem_ahb_hreadyout;
	reg ahb_rd;
	reg ahb_wr;
	reg[31:0] haddr;
	reg[1:0] hsize;
	wire[3:0] ahb_wb;
	wire ahb_ready;

	always @(negedge resetn or posedge aclk)
	begin
		if(resetn==0) begin
			mem_ahb_hreadyout <= 1'b1;
		end else begin
			if(mem_ahb_hreadyout && mem_ahb_htrans[1]) begin
				mem_ahb_hreadyout <= 1'b0;
				ahb_rd <= !mem_ahb_hwrite;
				ahb_wr <=  mem_ahb_hwrite;
				haddr <= mem_ahb_haddr;
				hsize <= mem_ahb_hsize[1:0];
			end else if(ahb_ready) begin
				mem_ahb_hreadyout <= 1'b1;
			end
		end
	end

	assign ahb_wb[0] = ( (hsize==2) || (hsize==1 && haddr[1]==0) || (hsize==0 && haddr[1:0]==2'b00) );
	assign ahb_wb[1] = ( (hsize==2) || (hsize==1 && haddr[1]==0) || (hsize==0 && haddr[1:0]==2'b01) );
	assign ahb_wb[2] = ( (hsize==2) || (hsize==1 && haddr[1]==1) || (hsize==0 && haddr[1:0]==2'b10) );
	assign ahb_wb[3] = ( (hsize==2) || (hsize==1 && haddr[1]==1) || (hsize==0 && haddr[1:0]==2'b11) );

	// 0x60000000
	wire sram_cs = (haddr[28:27]==2'b00);
	// 0x68000000
	wire pram_cs = (haddr[28:27]==2'b01);
	// 0x70000000
	wire sys_cs  = (haddr[28]==1); 

	assign mem_ahb_hrdata =
		(sram_cs)? sram_rdata :
		(pram_cs)? pram_rdata :
		(sys_cs )? sys_rdata :
		32'hffffffff;

	assign ahb_ready = sram_ready | sys_ready | pram_ready;
	assign mem_ahb_hresp = 1'b0;

///////////////////////////////////////////////////////
// System Ctrl                                       //
///////////////////////////////////////////////////////

	reg[31:0] treg0;
	reg[31:0] treg1;
	reg[31:0] treg2;
	reg[31:0] treg3;
	always @(negedge bclk)
	begin
		if(soc_spi_csn==0) begin
			treg0 <= {treg0[30:0], soc_spi_io0};
			treg1 <= {treg1[30:0], soc_spi_io1};
			treg2 <= {treg2[30:0], soc_spi_io2};
			treg3 <= {treg3[30:0], soc_spi_io3};
		end
	end

	wire sys_wr = (ahb_ack && sys_cs && ahb_wr);
	wire sys_rd = (ahb_start && sys_cs && ahb_rd);
	reg[31:0] sys_rdata;
	wire sys_ready = sys_cs && ahb_ack;

	always @(posedge bclk)
	begin
		if(sys_rd) begin
			case(haddr[4:2])
				3'd0:    sys_rdata <= treg0;
				3'd1:    sys_rdata <= treg1;
				3'd2:    sys_rdata <= treg2;
				3'd3:    sys_rdata <= treg3;
				3'd4:    sys_rdata <= taddr1;
				3'd6:    sys_rdata <= sys_reg3;
				3'd7:    sys_rdata <= sys_reg4;
				default: sys_rdata <= 32'hffffffff;
			endcase
		end
	end

	reg[31:0] sys_reg3;
	reg[31:0] sys_reg4;

	always @(negedge resetn or posedge bclk)
	begin
		if(resetn==0) begin
			sys_reg3 <= 32'h3456789a;
			sys_reg4 <= 32'h44556789;
		end else if(sys_wr) begin
			if(haddr[4:2]==3) sys_reg3 <= mem_ahb_hwdata;
			if(haddr[4:2]==4) sys_reg4 <= mem_ahb_hwdata;
		end
	end


///////////////////////////////////////////////////////
// SRAM                                              //
///////////////////////////////////////////////////////


	wire[3:0] sram_wb = ahb_wb;
	wire sram_wr = (ahb_ack && sram_cs && ahb_wr);
	wire[7:0] sram_waddr = haddr[9:2];

	wire sram_rd = (ahb_ack && sram_cs && ahb_rd);
	wire[7:0] sram_raddr = haddr[9:2];
	wire[31:0] sram_rdata;

	sync_dpram sram(sram_wb, bclk, mem_ahb_hwdata, sram_raddr, sram_rd, sram_waddr, sram_wr, sram_rdata);

	reg sram_ready;
	always @(posedge bclk)
	begin
		sram_ready <= sram_cs && ahb_ack;
	end


///////////////////////////////////////////////////////
// SPI PSRAM                                         //
///////////////////////////////////////////////////////


	reg pram_ready;
	reg[1:0] cmd_req;
	reg[31:0] pram_rdata;

	wire[31:0] cmd_dout;
	wire cmd_ack;
	wire data_valid;
	
	reg[31:0] taddr1;

	reg[1:0] pstate;
	localparam PS_IDLE=0, PS_WAITACK=1, PS_WAITDATA0=2, PS_WAITDATA1=3;

	always @(negedge resetn or posedge bclk)
	begin
		if(resetn==0) begin
			cmd_req <= 0;
			pstate <= PS_IDLE;
			pram_ready <= 0;
		end else begin
			case(pstate)
			PS_IDLE: begin
				pram_ready <= 0;
				if(ahb_ack && pram_cs) begin
					cmd_req <= (ahb_rd)? 2'd2: 2'd1;
					taddr1 <= haddr;
					pstate <= PS_WAITACK;
				end
			end
			PS_WAITACK: begin
				if(cmd_ack) begin
					cmd_req <= 0;
					if(cmd_req==2) begin
						pstate <= PS_WAITDATA0;
					end else begin
						pram_ready <= 1'b1;
						pstate <= PS_IDLE;
					end
				end
			end
			PS_WAITDATA0: begin
				if(data_valid) begin
					pram_rdata <=  (hsize==0)? {cmd_dout[ 7:0], cmd_dout[ 7:0], cmd_dout[7:0], cmd_dout[7:0]} :
										(hsize==1)? {cmd_dout[15:0], cmd_dout[15:0]} :
										cmd_dout;
					pstate <= PS_WAITDATA1;
				end
			end
			PS_WAITDATA1: begin
				if(data_valid) begin
					pram_ready <= 1'b1;
					pstate <= PS_IDLE;
				end
			end
			default: begin
				pstate <= PS_IDLE;
			end
			endcase
		end
	end


	wire ps_cs;
	wire[3:0] ps_din = {soc_spi_io3, soc_spi_io2, soc_spi_io1, soc_spi_io0};
	wire[3:0] ps_dout;
	wire[3:0] ps_oe;
	tpsram psram(resetn, bclk, aclk,
		cmd_req, cmd_ack, hsize, haddr[23:0], mem_ahb_hwdata, cmd_dout, data_valid,
		ps_cs, ps_din, ps_dout, ps_oe
	);

	assign soc_spi_csn = ps_cs;
	//assign soc_spi_clk = ~bclk;
	assign soc_spi_clk = 1'bz;
	assign soc_spi_io0 = (ps_oe[0])? ps_dout[0] : 1'bz;
	assign soc_spi_io1 = (ps_oe[1])? ps_dout[1] : 1'bz;
	assign soc_spi_io2 = (ps_oe[2])? ps_dout[2] : 1'bz;
	assign soc_spi_io3 = (ps_oe[3])? ps_dout[3] : 1'bz;


endmodule

