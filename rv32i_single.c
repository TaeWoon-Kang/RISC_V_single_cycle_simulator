/* **************************************
 * Module: top design of rv32i single-cycle processor
 *
 * Author:
 *
 * **************************************
 */

#include "rv32i.h"
#include "inst.h"

imem_output_t imem(imem_input_t imem_in, uint32_t *imem_data);
regfile_output_t regfile(regfile_input_t regfile_in, int32_t* reg_data);
alu_output_t alu(alu_input_t alu_in);
dmem_output_t dmem(dmem_input_t dmem_in, uint8_t* dmem_data0, uint8_t* dmem_data1, uint8_t* dmem_data2, uint8_t* dmem_data3);
control_output_t control(control_input_t control_in);
alu_control_output_t alu_control(alu_control_input_t alu_control_in);
//defined at inst.c & inst.h
//void print_inst(uint32_t inst); // for print the meaning of instruction

int debug_mode = 1; //if 1, debug mode, else if 0, non-debug mode
int save_new_dmem = 0; //if1, save the result of new dmem

int main (int argc, char *argv[]) {

	// get input arguments
	FILE *f_imem, *f_dmem0, *f_dmem1, *f_dmem2, *f_dmem3;
	if (argc < 6) {
		printf("usage: %s imem_data_file dmem0 dmem1 dmem2 dmem3\n", argv[0]);
		exit(1);
	}

	if ( (f_imem = fopen(argv[1], "r")) == NULL ) {
		printf("Cannot find %s\n", argv[1]);
		exit(1);
	}
	if ( (f_dmem0 = fopen(argv[2], "r")) == NULL ) {
		printf("Cannot find %s\n", argv[2]);
		exit(1);
	}

	if ( (f_dmem1 = fopen(argv[3], "r")) == NULL ) {
		printf("Cannot find %s\n", argv[3]);
		exit(1);
	}

	if ( (f_dmem2 = fopen(argv[4], "r")) == NULL ) {
		printf("Cannot find %s\n", argv[4]);
		exit(1);
	}
	
	if ( (f_dmem3 = fopen(argv[5], "r")) == NULL ) {
		printf("Cannot find %s\n", argv[5]);
		exit(1);
	}
	// memory data (global)
	// reg_data & dmem_dat changed to sign by Taewoon
	int32_t *reg_data;
	uint32_t *imem_data;
	uint8_t *dmem_data0;
	uint8_t *dmem_data1;
	uint8_t *dmem_data2;
	uint8_t *dmem_data3;

	reg_data = (int32_t*)malloc(32*sizeof(int32_t));
	imem_data = (uint32_t*)malloc(IMEM_DEPTH*sizeof(uint32_t));
	dmem_data0 = (uint8_t*)malloc(DMEM_DEPTH*sizeof(uint8_t)); //modified by taewoon to support unaligned memory
	dmem_data1 = (uint8_t*)malloc(DMEM_DEPTH*sizeof(uint8_t)); //modified by taewoon to support unaligned memory
	dmem_data2 = (uint8_t*)malloc(DMEM_DEPTH*sizeof(uint8_t)); //modified by taewoon to support unaligned memory
	dmem_data3 = (uint8_t*)malloc(DMEM_DEPTH*sizeof(uint8_t)); //modified by taewoon to support unaligned memory

	// initialize memory data
	int i, j, k;
	for (i = 0; i < 32; i++) reg_data[i] = 0;
	for (i = 0; i < IMEM_DEPTH; i++) imem_data[i] = 0;
	//initialize dmem
	for (i = 0; i < DMEM_DEPTH; i++) dmem_data0[i] = 0;
	for (i = 0; i < DMEM_DEPTH; i++) dmem_data1[i] = 0;
	for (i = 0; i < DMEM_DEPTH; i++) dmem_data2[i] = 0;
	for (i = 0; i < DMEM_DEPTH; i++) dmem_data3[i] = 0;

	uint32_t d, buf;
	i = 0;
	printf("\n*** Reading %s ***\n", argv[1]);
	while (fscanf(f_imem, "%1d", &buf) != EOF) {
		d = buf << 31;
		for (k = 30; k >= 0; k--) {
			if (fscanf(f_imem, "%1d", &buf) != EOF) {
				d |= buf << k;
			} else {
				printf("Incorrect format!!\n");
				exit(1);
			}
		}
		imem_data[i] = d;
		//printf("imem[%03d]: %08X\n", i, imem_data[i]);
		print_inst(d);
		i++;
	}

	i = 0;
	printf("\n*** Reading %s ***\n", argv[2]);
	while (fscanf(f_dmem0, "%2x", &buf) != EOF) {
		dmem_data0[i] = buf;
		printf("dmem0[%03d]: %02X\n", i, dmem_data0[i]);
		i++;
	}

	i = 0;
	printf("\n*** Reading %s ***\n", argv[3]);
	while (fscanf(f_dmem1, "%2x", &buf) != EOF) {
		dmem_data1[i] = buf;
		printf("dmem1[%03d]: %02X\n", i, dmem_data1[i]);
		i++;
	}

	i = 0;
	printf("\n*** Reading %s ***\n", argv[4]);
	while (fscanf(f_dmem2, "%2x", &buf) != EOF) {
		dmem_data2[i] = buf;
		printf("dmem2[%03d]: %02X\n", i, dmem_data2[i]);
		i++;
	}

	i = 0;
	printf("\n*** Reading %s ***\n", argv[5]);
	while (fscanf(f_dmem3, "%2x", &buf) != EOF) {
		dmem_data3[i] = buf;
		printf("dmem3[%03d]: %02X\n", i, dmem_data3[i]);
		i++;
	}


	fclose(f_imem);
	fclose(f_dmem0);
	fclose(f_dmem1);
	fclose(f_dmem2);
	fclose(f_dmem3);

	// processor model
	
	//signal for program counter
	uint32_t pc_curr = 0;
	uint32_t pc_next = 0;	// program counter
	uint32_t pc_next_plus4 = 0;
	uint32_t pc_next_branch = 0;
	uint8_t pc_next_sel = 0;
	uint8_t branch_mux = 0;

	imem_input_t imem_in;
	imem_output_t imem_out;
	
	dmem_input_t dmem_in;
	dmem_output_t dmem_out;

	regfile_input_t regfile_in;
	regfile_output_t regfile_out;

	alu_input_t alu_in;
	alu_output_t alu_out;

	control_output_t control_out;
	control_input_t control_in;

	alu_control_input_t alu_control_in;
	alu_control_output_t alu_control_out;

	uint32_t cc = 2;	// clock count
	pc_curr = 0;
	while (cc < CLK_NUM) {
		printf("\n**********clk %d**********\n",cc);
		// instruction fetch
		//imem input signal declare
		imem_in.addr = pc_curr/4;
		printf("Inst addr : %d\n",imem_in.addr);
		//end
		imem_out = imem(imem_in, imem_data);
			
		//make signal based instruction
		/*uint8_t opcode = (imem_out.dout & 0x7F); //make inst[6:0]
		uint8_t rd = (imem_out.dout & 0xF80)>>7; //make inst[11:7]
		uint8_t imm_low = (imem_out.dout & 0xF80)>>7; //make inst[11:7]
		uint8_t funct3 = (imem_out.dout & 0x6000)>>12; //make inst[14:12]
		uint8_t rs1 = (imem_out.dout & 0xF8000)>>15; //make inst[19:15] 
		uint8_t rs2 = (imem_out.dout & 0x1F00000)>>20; //make inst[24:20]
		uint8_t funct7 = (imem_out.dout & 0xFE000000)>>25; //make inst[31:25]
		uint16_t imm12 = ((funct7<<5)+rs2)>>20; //make inst[31:20]
		uint8_t imm_high = (imem_out.dout & 0xFE000000)>>25; //make inst[31:25]
		*/
		uint8_t opcode = (imem_out.dout & 0x7F); //make inst[6:0]
		uint8_t rd = (imem_out.dout >>7) & 0x1F; //make inst[11:7]
		uint8_t imm_low = (imem_out.dout >>7) & 0x1F; //make inst[11:7]
		uint8_t funct3 = (imem_out.dout >>12) & 7; //make inst[14:12]
		uint8_t rs1 = (imem_out.dout >>15) & 0x1F; //make inst[19:15] 
		uint8_t rs2 = (imem_out.dout >>20) & 0x1F; //make inst[24:20]
		uint8_t funct7 = (imem_out.dout >>25) & 0x7F; //make inst[31:25] 
		uint16_t imm12 = (imem_out.dout >>20) & 0xFFF; //make inst[31:20]
		uint8_t imm_high = (imem_out.dout >>25) & 0x7F; //make inst[31:25]

		// instruction decode
		
		// control input signal declare
		control_in.funct3 = funct3;
		control_in.opcode = opcode;
		// end
		
		control_out = control(control_in); //added by taewoon

		// regfile input signal declare
		//rd_din not declared at here, so just initial with zero
		regfile_in.rd_din = 0;
		//regfile_in.rd_din= (control_out.jal_check == 1 ) ? jal_rd_din : (control_out.mem_to_reg == 1) ? dmem_out.dout : alu_out.result;
		regfile_in.rs1 = rs1;
		regfile_in.rs2 = rs2;
		regfile_in.rd = rd; //It's okay to set rd now
		//don't set unitl write_back
		regfile_in.reg_write = 0;
		//regfile_in.reg_write = control_out.reg_write; 
		//end

		regfile_out = regfile(regfile_in, reg_data);
		
		//To show what does the inst.c mean
		uint8_t ret_inst = 0;
		ret_inst = print_inst(imem_out.dout);
		if(debug_mode)
		{
			if(ret_inst==1)
				break;
			else if(ret_inst==2)
			{
				printf("r%d : %d( 0x%x ) \n",regfile_in.rs1, reg_data[regfile_in.rs1], reg_data[regfile_in.rs1]);
			}
			else if(ret_inst==3)
			{
				printf("r%d : %d( 0x%x ) \n",regfile_in.rs1, reg_data[regfile_in.rs1], reg_data[regfile_in.rs1]);
			}
			else
			{
				printf("r%d : %d( 0x%x ) \n",regfile_in.rs1, reg_data[regfile_in.rs1], reg_data[regfile_in.rs1]);
				printf("r%d : %d( 0x%x ) \n",regfile_in.rs2, reg_data[regfile_in.rs2], reg_data[regfile_in.rs2]);
			}
		}

		//IMMEDIATE GENERATOR
		int32_t imm32 = 0;
		int32_t imm32_branch = 0;

		if(opcode == 0b0000011 || opcode == 0b0010011) //lw, imm arithmetic
				imm32 = imm12;//imm32 = {{20{inst[31]}},inst[31:20]};
		else if (opcode == 0b0100011) //store
				imm32 = (imm_high<<5)+imm_low;//imm32 = {{20{inst[31]}}, inst[31:25], inst[11:7]};
		else if (opcode == 0b0110111 || opcode == 0b0010111) //LUI or AUIP
				imm32 = imem_out.dout & 0x000;//imm32 = {inst[31:12], 12'h000 };
		else if (opcode == 0b1100111)
				imm32 = imm12;//imm32 = {{20{inst[31]}},inst[31:20]};
		else
				imm32 = 0;//imm32 = 32'h000;
		
		if(opcode == 0b0010111) // AUIPC
			imm32_branch = imem_out.dout & 0xFFFFF000;//imm32 = {inst[31:12], 12'h000 };
		else if(opcode == 0b1100011) //SB-type
			imm32_branch = ((((imem_out.dout>>7)&1)<<11) + (((imem_out.dout>>25)&0x3F)<<5) + (((imem_out.dout>>8)&0xF)<<1));
			//imm32_branch = {{20{inst[31]}}, inst[7], inst[30:25], inst[11:8], 1'b0};
		else if(opcode == 0b1101111 || opcode == 0b1100111) //UJ-type
			imm32_branch = ((((imem_out.dout>>12)&0xFF)<<12) + (((imem_out.dout>>20)&1)<<11) + (((imem_out.dout>>21)&0x3FF)<<1));
			//imm32_branch = {{12{inst[31]}}, inst[19:12], inst[20], inst[30:25], inst[24:21], 1'b0};
		else
			imm32_branch = 0;

		// execution
		
		//Make the alu_control signal
		alu_control_in.opcode = opcode;
		alu_control_in.alu_op = control_out.alu_op;
		alu_control_in.funct3 = funct3;
		alu_control_in.funct7 = funct7;

		alu_control_out = alu_control(alu_control_in);

		//alu input signal declare
		alu_in.in1 = regfile_out.rs1_dout;	
		alu_in.in2 = (control_out.alu_src == 1) ? imm32 : regfile_out.rs2_dout;
		alu_in.alu_control = alu_control_out.alu_control;
		//end

		alu_out = alu(alu_in);
		
		uint32_t jal_rd_din = 0;
		if(((opcode>>5) & 0x3)==0b11)
			jal_rd_din = pc_curr + 4;
		else if(((opcode>>5) & 0x3)==0b01)
			jal_rd_din = imm32;
		else if(((opcode>>5) & 0x3)==0b00)
			jal_rd_din = pc_curr+imm32;
		else
			jal_rd_din = 0;
		//program counter value

		//branch_muxing with pc select
		if(control_out.branch0 == 1 && alu_out.zero == 1)
			branch_mux = 1;
		else if(control_out.branch1 == 1 && alu_out.zero == 0)
			branch_mux = 1;
		else if(control_out.branch2 == 1 && alu_out.sign == 1)
			branch_mux = 1;
		else if(control_out.branch3 == 1 && (alu_out.sign == 0||alu_out.zero == 1))
			branch_mux = 1;
		else if(control_out.branch4 == 1)
			branch_mux = 1;
		else
			branch_mux = 0;		

		if(debug_mode)
			printf("pc_curr : %d, inst_address : %d\n",pc_curr,pc_curr>>2);
		pc_next_plus4 = pc_curr+4;
		pc_next_sel = (branch_mux == 1) ? 1 : 0;
		pc_next_branch = (opcode == 0b1100111) ? imm32 + regfile_out.rs1_dout : pc_curr + imm32_branch;
		pc_next = (pc_next_sel) ? pc_next_branch : pc_next_plus4;
		pc_curr = pc_next;
		if(debug_mode)
			printf("pc_next : %d, inst_address : %d\n",pc_curr,pc_curr>>2);

		//end
		//end

		// memory

		//dmem input signal declare
		dmem_in.addr = alu_out.result;
		if(debug_mode==1 && (opcode == 0b0000011 || opcode == 0b0100011))
			printf("dmem addr: %d\n",dmem_in.addr);
		dmem_in.din = regfile_out.rs2_dout;
		dmem_in.rd_en = control_out.mem_read;
		dmem_in.wr_en = control_out.mem_write;
		dmem_in.sz = (funct3 & 0x3);
		dmem_in.sign = (funct3 & 0x4) >> 2;
		//end

		dmem_out = dmem(dmem_in, dmem_data0, dmem_data1, dmem_data2, dmem_data3);

		// write-back	
		//set for signal
		if(control_out.reg_write == 1)
		{
			if(debug_mode)
				printf("write back to register file\n");
			regfile_in.reg_write = control_out.reg_write; 
			regfile_in.rd_din= (control_out.jal_check == 1 ) ? jal_rd_din : (control_out.mem_to_reg == 1) ? dmem_out.dout : alu_out.result;
			regfile(regfile_in,reg_data);
		}

		if(opcode != 0b1100011 && debug_mode == 1)
			printf("dest reg-> r%d = %d( %x )\n",regfile_in.rd, reg_data[regfile_in.rd], reg_data[regfile_in.rd]);

		cc++;
	}

	//Add by taewoon for save New dmem
	if(save_new_dmem){
		FILE *nfp = fopen("dmem_result0.mem","w");
		for(int wr=32; wr>=0; wr--)
		{
			fprintf(nfp,"%d: %x  ",wr,dmem_data0[wr]);
			fprintf(nfp,"\n");
		}
		FILE *nfp1 = fopen("dmem_result1.mem","w");
		for(int wr=32; wr>=0; wr--)
		{
			fprintf(nfp1,"%d: %x  ",wr,dmem_data1[wr]);
			fprintf(nfp1,"\n");
		}
		FILE *nfp2 = fopen("dmem_result2.mem","w");
		for(int wr=32; wr>=0; wr--)
		{
			fprintf(nfp2,"%d: %x  ",wr,dmem_data2[wr]);
			fprintf(nfp2,"\n");
		}
		FILE *nfp3 = fopen("dmem_result3.mem","w");
		for(int wr=32; wr>=0; wr--)
		{
			fprintf(nfp3,"%d: %x  ",wr,dmem_data3[wr]);
			fprintf(nfp3,"\n");
		}
		fclose(nfp);
		fclose(nfp1);
		fclose(nfp2);
		fclose(nfp3);
	}
	for(int i=0; i<32; i++)
	{
		printf("r%d= %d(%x)  ",i,reg_data[i],reg_data[i]);
		if(i%4 == 0 && i!=0)
			printf("\n");
	}

	free(reg_data);
	free(imem_data);

	free(dmem_data0);
	free(dmem_data1);
	free(dmem_data2);
	free(dmem_data3);

	printf("\n");

	return 1;
}

imem_output_t imem(imem_input_t imem_in, uint32_t *imem_data)
{
	imem_output_t result;
	result.dout = imem_data[imem_in.addr];
	return result;
}
regfile_output_t regfile(regfile_input_t regfile_in, int32_t* reg_data)
{
	regfile_output_t result;
	if(regfile_in.reg_write == 1)
	{
		if(regfile_in.rd != 0)
			reg_data[regfile_in.rd] = regfile_in.rd_din;
	}
	else
	{
		result.rs1_dout = reg_data[regfile_in.rs1];
		result.rs2_dout = reg_data[regfile_in.rs2];
	}
	return result;
}
alu_output_t alu(alu_input_t alu_in)
{
	alu_output_t result;
	uint32_t u_in1, u_in2;
	u_in1 = (uint32_t)alu_in.in1;
	u_in2 = (uint32_t)alu_in.in2;
	switch(alu_in.alu_control)
	{
		case AND: 
			result.result = alu_in.in1 & alu_in.in2;
			break;
		case OR: 
			result.result = alu_in.in1 | alu_in.in2;
			break;
		case ADD: 
			result.result = alu_in.in1 + alu_in.in2;
			break;
		case SUB: 
			result.result = alu_in.in1 - alu_in.in2;
			break;
		case XOR: 
			result.result = alu_in.in1 ^ alu_in.in2;
			break;
		case SLL: 
			result.result = alu_in.in1 << alu_in.in2;
			break;
		case SRL: 
			result.result = alu_in.in1 >> alu_in.in2;
			break;
		case SRA: 
			result.result = alu_in.in1 >> alu_in.in2;
			break;
		case SLT: 
			result.result = (alu_in.in1 > alu_in.in2) ? 1 : 0;
			break;
		case SLU: 
			result.result = (u_in1 > u_in2) ? 1:0;
			break;
	}
	result.zero = (result.result == 0)?1:0;
	result.sign = (result.result<0)?1:0;
	return result;
}

dmem_output_t dmem(dmem_input_t dmem_in, uint8_t* dmem_data0, uint8_t* dmem_data1, uint8_t* dmem_data2, uint8_t* dmem_data3)
{
	dmem_output_t result;
	int32_t full_data;
	uint16_t addr0, addr1, addr2, addr3;
	uint8_t low_3bit_addr = dmem_in.addr & 0x3;
	int32_t dout_tmp = 0;
	uint32_t tmp_dout0, tmp_dout1, tmp_dout2, tmp_dout3 = 0;

	//address preprocessing

	if((dmem_in.addr & 0x3)==0)
	{
		addr0 = (dmem_in.addr>>2);
		addr1 = (dmem_in.addr>>2);
		addr2 = (dmem_in.addr>>2);
		addr3 = (dmem_in.addr>>2);
	}
	else if((dmem_in.addr & 0x3)==1)
	{
		addr0 = (dmem_in.addr>>2)+1;
		addr1 = (dmem_in.addr>>2);
		addr2 = (dmem_in.addr>>2);
		addr3 = (dmem_in.addr>>2);
	}
	else if((dmem_in.addr & 0x3)==2)
	{
		addr0 = (dmem_in.addr>>2)+1;
		addr1 = (dmem_in.addr>>2)+1;
		addr2 = (dmem_in.addr>>2);
		addr3 = (dmem_in.addr>>2);
	}
	else if((dmem_in.addr & 0x3)==3)
	{
		addr0 = (dmem_in.addr>>2)+1;
		addr1 = (dmem_in.addr>>2)+1;
		addr2 = (dmem_in.addr>>2)+1;
		addr3 = (dmem_in.addr>>2);
	}
	else
	{
		addr0 = 0;
		addr1 = 0;
		addr2 = 0;
		addr3 = 0;
	}

	//output

	tmp_dout0 = dmem_data0[addr0];
	tmp_dout1 = dmem_data1[addr1];
	tmp_dout2 = dmem_data2[addr2];
	tmp_dout3 = dmem_data3[addr3];
	int32_t tmp =0;

	switch(dmem_in.addr & 0x3)
	{
		case 0b00 :
			dout_tmp = (((int32_t)tmp_dout3 << 24) + (tmp_dout2 << 16) + (tmp_dout1 << 8) + tmp_dout0); 
			break;
		case 0b01 :
			dout_tmp = (((int32_t)tmp_dout0 << 24) + (tmp_dout3 << 16) + (tmp_dout2 << 8) + tmp_dout1); 
			break;
		case 0b10 :
			dout_tmp = (((int32_t)tmp_dout1 << 24) + (tmp_dout0 << 16) + (tmp_dout3 << 8) + tmp_dout2); 
			break;
		case 0b11 :
			dout_tmp = (((int32_t)tmp_dout2 << 24) + (tmp_dout1 << 16) + (tmp_dout0 << 8) + tmp_dout3); 
			break;
	}

	if (dmem_in.rd_en == 1) {
		if(dmem_in.sz == 0 && dmem_in.sign == 0){
			//dout = {{24{dout_tmp[7]}},dout_tmp[7:0]};
			result.dout = ((int32_t)(dout_tmp>>24));
		}
		else if(dmem_in.sz == 1 && dmem_in.sign == 0) {
			//dout = {{16{dout_tmp[15]}},dout_tmp[15:0]};
			result.dout = ((int32_t)(dout_tmp>>16));
		}
		else if(dmem_in.sz == 2) {
			//dout = dout_tmp;
			result.dout = dout_tmp;
		}
		else if (dmem_in.sz == 0 && dmem_in.sign == 1) {
			//dout = {{24{1'b0}},dout_tmp[7:0]};
			result.dout = ((uint32_t)(dout_tmp>>24));
		}
		else if(dmem_in.sz == 1 && dmem_in.sign == 1) {
			//dout = {{16{1'b0}},dout_tmp[15:0]};	
			result.dout = ((uint32_t)(dout_tmp>>16));
		}
		else
			result.dout = 0;
	}
	else
		result.dout = 0;

	if(dmem_in.rd_en == 1)
	{
		printf("addr0 = %d, addr1 = %d, addr2 = %d, addr3 = %d\n",addr0,addr1,addr2,addr3);
		printf("output(load) data of DMEM : %d( %x )\n",result.dout,result.dout);
	}
	//output end

	//input

	int32_t din_tmp=0;
	uint8_t we;
	uint8_t part0,part1,part2,part3 = 0;
	part0 = ((dmem_in.din>> 24)&0xFF); //din[31:24]
	part1 = ((dmem_in.din>> 16)&0xFF); //din[23:16]
	part2 = ((dmem_in.din>> 8)&0xFF); //din[15:8]
	part3 = dmem_in.din & 0xFF; //din[7:0]

	switch(dmem_in.addr & 0x3)
	{
		case 0b00:
			din_tmp = dmem_in.din;
			break;
		case 0b01:
			din_tmp = (part1<<24) + (part2<<16) + (part3<<8) + part0; 	
			break;
		case 0b10:
			din_tmp = (part2<<24) + (part3<<16) + (part0<<8) + part1; 	
			break;
		case 0b11:
			din_tmp = (part3<<24) + (part0<<16) + (part1<<8) + part2; 	
			break;
	}

	if(dmem_in.wr_en == 1)
	{
		if(dmem_in.sz == 0)
		{
			switch(dmem_in.addr & 0x3)
			{
				case 0: 
					we =1;
					break;
				case 2:
					we = 2;
					break;
				case 3:
					we = 4;
					break;
				case 4:
					we = 8;
					break;
				default:
					we =0;
					break;
			}
		}
		else if(dmem_in.sz == 1)
		{
			switch(dmem_in.addr & 0x3)
			{
				case 0:
					we = 3; //0b0011
					break;
				case 1:
					we = 6; //0b0110
					break;
				case 2:
					we = 12; //0b1100
					break;
				case 3:
					we = 9; //0b1001
					break;
				default:
					we = 0;
					break;
			}
		}
		else
			we = 15; //0b1111
	}
	else
		we = 0;

	if(we & 1) //we == xxx1
		dmem_data0[addr0] = (din_tmp & 0xFF);
	if(((we>>1) & 1) == 1) //we == xx1x
		dmem_data1[addr1] = ((din_tmp>>8) & 0xFF);
	if(((we>>2) & 1) == 1) //we == x1xx
		dmem_data2[addr2] = ((din_tmp>>16) & 0xFF);	
	if(((we>>3) & 1) == 1) //we == 1xxx
		dmem_data3[addr3] = ((din_tmp>>24) & 0xFF);	

	return result;
}

control_output_t control(control_input_t control_in)
{
	control_output_t result;
	uint8_t shifted_opcode = control_in.opcode>>4;
	result.branch0 = (shifted_opcode==0b110 && control_in.funct3 == 0b000 && (control_in.opcode & 0x08)!= 0x08 ) ? 1 : 0;
	result.branch1 = (shifted_opcode==0b110 && control_in.funct3 == 0b001) ? 1 : 0;
	result.branch2 = (shifted_opcode==0b110 && (control_in.funct3 == 0b100 || control_in.funct3==0b110)) ? 1 : 0;
	result.branch3 = (shifted_opcode==0b110 && (control_in.funct3 == 0b101 || control_in.funct3==0b111)) ? 1 : 0;
	result.branch4 = ((control_in.opcode == 0b1101111 || control_in.opcode == 0b1100111) && control_in.funct3 == 0) ? 1 : 0;
	result.mem_read = (shifted_opcode==0b000) ? 1 : 0;
	result.mem_write = (shifted_opcode==0b010) ? 1 : 0;
	result.mem_to_reg = (shifted_opcode==0b000) ? 1 : 0;
	result.reg_write = (shifted_opcode == 0b000 ||shifted_opcode == 0b001 || shifted_opcode == 0b011 || (control_in.opcode & 0x08) == 0b111) ? 1 : 0;
	result.alu_src = (shifted_opcode == 0b000||shifted_opcode == 0b001 || shifted_opcode == 0b010)? 1 : 0;
	uint8_t alu_op0 = (shifted_opcode == 0b110) ? 1 : 0;
	uint8_t alu_op1 = (shifted_opcode == 0b001||shifted_opcode == 0b011) ? 1 : 0;
	result.alu_op = (alu_op1<<1)+alu_op0;
	result.jal_check = ((control_in.opcode & 0x7) == 0b111) ? 1:0; 
	return result;
}

alu_control_output_t alu_control(alu_control_input_t alu_control_in)
{
	alu_control_output_t result;
	if(alu_control_in.alu_op == 0b00)
				result.alu_control = 0b0010;
		else if(alu_control_in.alu_op == 0b01)
				result.alu_control = 0b0110;
		else if(alu_control_in.alu_op == 0b10) 
				if((alu_control_in.opcode & 0x20)>>5 == 0b1) //opcode[5]
						if((alu_control_in.funct7 & 0x20)>>5 == 0b0) //funct7[5]
								if( alu_control_in.funct3 == 0b000)
										result.alu_control = 0b0010;  //add
								else if(alu_control_in.funct3 == 0b001)
										result.alu_control = 0b1000;  // Shift Left Logical
								else if(alu_control_in.funct3 == 0b101)
										result.alu_control = 0b1001;  // Shift Right Logical
								else if(alu_control_in.funct3 == 0b111)
										result.alu_control = 0b0000;  //and
								else if(alu_control_in.funct3 == 0b110)
										result.alu_control = 0b0001;  //or
								else if(alu_control_in.funct3 == 0b100)
										result.alu_control = 0b0011;  //xor
								else if(alu_control_in.funct3 == 0b010)
										result.alu_control = 0b1100;  //Set less Than
								else if(alu_control_in.funct3 == 0b011)
										result.alu_control = 0b1101;  //Set less Than unsigned
								else
										result.alu_control = 0b1111;
						
						else if((alu_control_in.funct7 & 0x20)>>5 == 0b1) 
								if(alu_control_in.funct3 == 0b000)
										result.alu_control = 0b0110; //Subtraction
								else if(alu_control_in.funct3 == 0b101)
										result.alu_control = 0b1010; //Shift Right Arithmatic
								else
										result.alu_control = 0b1111;
						else
								result.alu_control = 0b1111;
				else 
						if( alu_control_in.funct3 == 0b000)
								result.alu_control = 0b0010;  //add immediate
						else if(alu_control_in.funct3 == 0b001)
								result.alu_control = 0b1000;  // Shift Left Logical immediate
						else if(alu_control_in.funct3 == 0b101)
								result.alu_control = 0b1001;  // Shift Right Logical immediate
						else if(alu_control_in.funct3 == 0b111)
								result.alu_control = 0b0000;  //and immediate
						else if(alu_control_in.funct3 == 0b110)
								result.alu_control = 0b0001;  //or immediate
						else if(alu_control_in.funct3 == 0b100)
								result.alu_control = 0b0011;  //xor immediate
						else if(alu_control_in.funct3 == 0b010)
								result.alu_control = 0b1100;  //Set less Than
						else if(alu_control_in.funct3 == 0b011)
								result.alu_control = 0b1101;  //Set less Than unsigned
						else
								result.alu_control = 0b1111;
		else
				result.alu_control = 0b1111;
		return result;
}

