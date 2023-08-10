///////////////////////////////////////////////////////
// Module: Tiny PSRAM Controller                     //
///////////////////////////////////////////////////////

module tpsram(
	reset,
	clk,
	clk2x,

	cmd_req,
	cmd_ack,
	cmd_size,
	cmd_addr,
	cmd_din,
	cmd_dout,
	data_valid,

	ps_cs,
	ps_din,
	ps_dout,
	ps_oe
);

// cmd_req:
//   00: nop
//   01: write
//   10: read
// cmd_size:
//   00: byte
//   01: word
//   10: dword


	input reset;
	input clk;
	input clk2x;

	input [ 1:0] cmd_req;
	output cmd_ack;
	input [ 1:0] cmd_size;
	input [23:0] cmd_addr;
	input [31:0] cmd_din;
	output[31:0] cmd_dout;
	output data_valid;

	output ps_cs;
	input [3:0] ps_din;
	output[3:0] ps_dout;
	output[3:0] ps_oe;


///////////////////////////////////////////////////////
// QSPI state                                        //
///////////////////////////////////////////////////////

	reg[3:0] p_state;
	reg[3:0] p_next;

	reg ps_cs;
	reg[3:0] ps_dout;
	reg[3:0] ps_oe;

	reg data_valid;
	reg cmd_ack;

	reg[31:0] p_cad;
	reg[7:0] ps_cmd;
	reg[3:0] por_cnt;
	reg[4:0] rw_cnt;
	reg[31:0] rdata;
	reg[31:0] wdata;
	reg[2:0] p_wcnt;


	localparam CMD_IDLE=2'd0, CMD_WRITE=2'd1, CMD_READ=2'd2;

	localparam  P_IDLE=4'd0, P_POR=4'd1, P_CADDR=4'd2, P_READ_WAIT=4'd3, P_READ_DATA=4'd4,
				P_WRITE=4'd5, P_FINISH=4'd6;

	always @(negedge reset or posedge clk)
	begin
		if(reset==0) begin
			p_state <= P_POR;
			ps_oe <= 4'b0000;
			ps_cmd <= 8'h35;
			ps_cs <= 1'b1;
			por_cnt <= 4'd8;
			rw_cnt <= 5'b0;
			cmd_ack <= 1'b0;
		end else begin
			case(p_state)
			P_POR: begin
				ps_cs <= 1'b0;
				ps_oe <= 4'b0001;
				ps_dout[0] <= ps_cmd[7];
				ps_cmd <= {ps_cmd[6:0], 1'b0};
				por_cnt <= por_cnt-1'b1;
				if(por_cnt==0) begin
					ps_cs <= 1'b1;
					ps_oe <= 4'b0000;
					p_state <= P_IDLE;
				end
			end

			P_IDLE: begin
				rw_cnt <= 0;
				if(cmd_req==CMD_READ) begin
					cmd_ack <= 1;
					p_cad <= {8'heb, cmd_addr[23:0]};
					p_state <= P_CADDR;
					p_next  <= P_READ_WAIT;
				end else if(cmd_req==CMD_WRITE) begin
					cmd_ack <= 1;
					p_cad <= {8'h02, cmd_addr[23:0]};
					wdata[ 7: 0] <= cmd_din[31:24];
					wdata[15: 8] <= cmd_din[23:16];
					wdata[23:16] <= (cmd_addr[1  ]==0)? cmd_din[15: 8] : cmd_din[31:24];
					wdata[31:24] <= (cmd_addr[1:0]==0)? cmd_din[ 7: 0] :
									(cmd_addr[1:0]==1)? cmd_din[15: 8] :
									(cmd_addr[1:0]==2)? cmd_din[23:16] :
									                    cmd_din[31:24] ;
					p_wcnt <= (cmd_size==2)? 3'd7 : (cmd_size==1)? 3'd3 : 3'd1;
					p_state <= P_CADDR;
					p_next  <= P_WRITE;
				end
			end

			P_CADDR: begin
				cmd_ack <= 0;
				ps_cs <= 1'b0;
				ps_oe <= 4'b1111;
				ps_dout <= p_cad[31:28];
				p_cad <= {p_cad[27:0], 4'b0000};
				if(rw_cnt==7) begin
					p_state <= p_next;
				end
				rw_cnt <= rw_cnt+1'b1;
			end

			P_READ_WAIT: begin
				ps_oe <= 4'b0000;
				rw_cnt <= rw_cnt+1'b1;
				if(rw_cnt==15) begin
					p_state <= P_READ_DATA;
				end
			end

			P_READ_DATA: begin
				rw_cnt <= rw_cnt+1'b1;
				data_valid <= (rw_cnt[2:0]==3'b110);
				if(rw_cnt==31) begin
					ps_cs <= 1'b1;
					p_state <= P_FINISH;
				end
			end

			P_WRITE: begin
				ps_dout <= wdata[31:28];
				wdata <= {wdata[27:0], 4'b0000};
				p_wcnt <= p_wcnt-1'b1;
				if(p_wcnt==0)
					p_state <= P_FINISH;
			end

			P_FINISH: begin
				data_valid <= 0;
				ps_cs <= 1'b1;
				ps_oe <= 4'b0000;
				p_state <= P_IDLE;
			end

			default: begin
				p_state <= P_IDLE;
			end
			endcase;

		end
	end

//	reg[3:0] ps_din_d;
//	always @(posedge clk2x)
//	begin
//		ps_din_d <= ps_din;
//	end

	always @(negedge clk)
	begin
		rdata <= {rdata[27:0], ps_din};
	end

	assign cmd_dout = {rdata[7:0], rdata[15:8], rdata[23:16], rdata[31:24]};

endmodule

